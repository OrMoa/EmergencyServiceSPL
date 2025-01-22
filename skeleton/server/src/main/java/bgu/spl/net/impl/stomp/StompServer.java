package bgu.spl.net.impl.stomp;

import java.util.concurrent.ConcurrentHashMap;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Server;
import bgu.spl.net.impl.stomp.StompMessageEncoderDecoder;
import bgu.spl.net.impl.stomp.StompMessagingProtocolImpl;

public class StompServer {

    public static void main(String[] args) {// Determine the port from the command-line arguments, default to 7777
        int port = args.length > 1 ? Integer.parseInt(args[1]) : 7777;

        if (args.length > 0 && args[0].equalsIgnoreCase("reactor")) { 
            // If the first argument is "reactor", run the server in Reactor pattern
            int numThreads = Runtime.getRuntime().availableProcessors(); // Number of available CPU cores
            Server.<String>reactor(
                    numThreads, // Number of threads
                    port, // Port for the server to listen on
                    StompMessagingProtocolImpl::new, // Factory for the protocol
                    StompMessageEncoderDecoder::new // Factory for message encoding/decoding
            ).serve();
        } else { 
            // Default to Thread-Per-Client pattern
            Server.<String>threadPerClient(
                    port, // Port for the server to listen on
                    StompMessagingProtocolImpl::new, // Factory for the protocol
                    StompMessageEncoderDecoder::new // Factory for message encoding/decoding
            ).serve();
        }
    }
}
