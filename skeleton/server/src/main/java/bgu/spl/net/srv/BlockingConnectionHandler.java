package bgu.spl.net.srv;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.MessagingProtocol;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.net.Socket;

/*This pattern corresponds to the Thread-Per-Client (TPC) pattern,
 where each client is handled by a separate thread.*/

public class BlockingConnectionHandler<T> implements Runnable, ConnectionHandler<T> {

    //link between client and server, processing the messages received from the client
    private final MessagingProtocol<T> protocol;
    //Converts between textual/binary messages and objects (encoding and decoding)
    private final MessageEncoderDecoder<T> encdec;

    private final Socket sock;
    private BufferedInputStream in;
    private BufferedOutputStream out;
    private volatile boolean connected = true;

    public BlockingConnectionHandler(Socket sock, MessageEncoderDecoder<T> reader, MessagingProtocol<T> protocol) {
        this.sock = sock;
        this.encdec = reader;
        this.protocol = protocol;
    }

    @Override
    public void run() {
        try (Socket sock = this.sock) { //just for automatic closing
            int read;

            in = new BufferedInputStream(sock.getInputStream());
            out = new BufferedOutputStream(sock.getOutputStream());

            while (!protocol.shouldTerminate() && connected && (read = in.read()) >= 0) {
                T nextMessage = encdec.decodeNextByte((byte) read);
                if (nextMessage != null) {
                    T response = protocol.process(nextMessage);
                    if (response != null) {
                        out.write(encdec.encode(response));
                        out.flush();
                    }
                }
            }

        } catch (IOException ex) {
            ex.printStackTrace();
        }

    }

    @Override
    public void close() throws IOException {
        connected = false;
        sock.close();
    }

    @Override
    public void send(T msg) {
        synchronized (this) { // Synchronize to ensure thread-safe access to the connection
            try {
            byte[] encodedMessage = encdec.encode(msg); // Encode the message into bytes
            out.write(encodedMessage); // Write the encoded message to the output stream
            out.flush(); // Immediately flush the data to the client
            } catch (IOException e) {
            System.err.println("Failed to send message: " + e.getMessage()); // Log the error
            e.printStackTrace();
            connected = false; // Mark the connection as no longer active
            }
        }
    }

}
