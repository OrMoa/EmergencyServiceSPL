package bgu.spl.net.srv;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.MessagingProtocol;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.channels.ClosedSelectorException;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.function.Supplier;

public class Reactor<T> implements Server<T> {

    private final int port;
    private final Supplier<MessagingProtocol<T>> protocolFactory;
    private final Supplier<MessageEncoderDecoder<T>> readerFactory;
    private final ActorThreadPool pool;
    private Selector selector;

    //added
    private volatile boolean shutdown;
    private ConnectionsImpl<T> connections;
    private int connectionIdCounter;
    //

    private Thread selectorThread;
    private final ConcurrentLinkedQueue<Runnable> selectorTasks = new ConcurrentLinkedQueue<>();

    public Reactor(
            int numThreads,
            int port,
            Supplier<MessagingProtocol<T>> protocolFactory,
            Supplier<MessageEncoderDecoder<T>> readerFactory) {

        this.pool = new ActorThreadPool(numThreads);
        this.port = port;
        this.protocolFactory = protocolFactory;
        this.readerFactory = readerFactory;
        //added
        this.connections = new ConnectionsImpl<>();
        this.connectionIdCounter = 0;
        this.shutdown = false;
        System.out.println("[DEBUG][Reactor] Reactor initialization complete");

        //
    }

    @Override
    public void serve() {
	selectorThread = Thread.currentThread();
    System.out.println("[DEBUG][Reactor] Starting server on selector thread: " + selectorThread.getName());
        try (Selector selector = Selector.open();
                ServerSocketChannel serverSock = ServerSocketChannel.open()) {

            this.selector = selector; //just to be able to close

            serverSock.bind(new InetSocketAddress(port));
            serverSock.configureBlocking(false);
            serverSock.register(selector, SelectionKey.OP_ACCEPT);
			System.out.println("Server started");

            while (!Thread.currentThread().isInterrupted()) {

                selector.select();
                runSelectionThreadTasks();

                for (SelectionKey key : selector.selectedKeys()) {

                    if (!key.isValid()) {
                        System.out.println("[DEBUG][Reactor] Invalid key found, skipping");
                        continue;
                    } else if (key.isAcceptable()) {
                        System.out.println("[DEBUG][Reactor] Accepting new connection");
                        handleAccept(serverSock, selector);
                    } else {
                        System.out.println("[DEBUG][Reactor] Handling read/write operation");
                        handleReadWrite(key);
                    }
                }

                selector.selectedKeys().clear(); //clear the selected keys set so that we can know about new events

            }

        } catch (ClosedSelectorException ex) {
            //do nothing - server was requested to be closed
        } catch (IOException ex) {
            //this is an error
            ex.printStackTrace();
        }

        System.out.println("server closed!!!");
        pool.shutdown();
    }

    /*package*/ void updateInterestedOps(SocketChannel chan, int ops) {
        final SelectionKey key = chan.keyFor(selector);
        if (Thread.currentThread() == selectorThread) {
            System.out.println("[DEBUG][Reactor] Directly updating ops for channel on selector thread");
            key.interestOps(ops);
        } else {
            System.out.println("[DEBUG][Reactor] Queueing ops update task for selector thread");
            selectorTasks.add(() -> {
                key.interestOps(ops);
            });
            selector.wakeup();
        }
    }


    private void handleAccept(ServerSocketChannel serverChan, Selector selector) throws IOException {
        /*SocketChannel clientChan = serverChan.accept();
        clientChan.configureBlocking(false);
        final NonBlockingConnectionHandler<T> handler = new NonBlockingConnectionHandler<>(
                readerFactory.get(),
                protocolFactory.get(),
                clientChan,
                this);
        //added
        connections.addConnection(connectionId, handler);
        handler.getProtocol().start(connectionId, connections);
        connectionId++;
        System.out.println("new client connected");
        //
        clientChan.register(selector, SelectionKey.OP_READ, handler);*/
        SocketChannel clientChan = serverChan.accept();
        clientChan.configureBlocking(false);
        
        final MessagingProtocol<T> protocol = protocolFactory.get();
        final int connectionId = connectionIdCounter++;
        
        System.out.println("[DEBUG][Reactor] New client connected. Assigned ID: " + connectionId);
        
        final NonBlockingConnectionHandler<T> handler = new NonBlockingConnectionHandler<>(
                readerFactory.get(),
                protocol,
                clientChan,
                this);

        connections.addClient(connectionId, handler);
        protocol.start(connectionId, connections);
        
        clientChan.register(selector, SelectionKey.OP_READ, handler);
        System.out.println("[DEBUG][Reactor] Client handler registered for reading");
    }

    private void handleReadWrite(SelectionKey key) {
        @SuppressWarnings("unchecked")
        NonBlockingConnectionHandler<T> handler = (NonBlockingConnectionHandler<T>) key.attachment();

        if (key.isReadable()) {
            System.out.println("[DEBUG][Reactor] Processing read operation");
            Runnable task = handler.continueRead();
            if (task != null) {
                System.out.println("[DEBUG][Reactor] Submitting read task to thread pool");
                pool.submit(handler, task);
            }
        }

	    if (key.isValid() && key.isWritable()) {
            System.out.println("[DEBUG][Reactor] Processing write operation");
            handler.continueWrite();
        }
    }

    private void runSelectionThreadTasks() {
        while (!selectorTasks.isEmpty()) {
            selectorTasks.remove().run();
        }
    }


    @Override
    public void close() throws IOException {
        selector.close();
    }

}
