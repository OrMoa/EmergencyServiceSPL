package bgu.spl.net.api;

import bgu.spl.net.srv.Connections;

public interface StompMessagingProtocol<T> extends MessagingProtocol<T> {
	/**
	 * Used to initiate the current client protocol with it's personal connection ID and the connections implementation
	**/

    @Override
    void start(int connectionId, Connections<T> connections);
    
    //void process(T message);
    default T process(T message) {
        processInternal(message); // Call a void method internally
        return null; // Since MessagingProtocol expects a return, return null
    }

    void processInternal(T message);
	
	/**
     * @return true if the connection should be terminated
     */
    @Override
    boolean shouldTerminate();
}
