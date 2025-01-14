package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;

public class ConnectionsImpl<T> implements Connections<T> {

    /*
     * a map that connect between connectionId and ConnectionHandler<T>
     * (object that represent the connection between server aand client)
     */
    private ConcurrentHashMap<Integer, ConnectionHandler<T>> clients = new ConcurrentHashMap<>();
    private ConcurrentHashMap<String, ConcurrentHashMap<Integer, String>> channels = new ConcurrentHashMap<>();

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = clients.get(connectionId);
        if (handler != null) {
            handler.send(msg);
            return true;
        }
        return false;
    }

    @Override
    public void send(String channel, T msg) {
        ConcurrentHashMap<Integer, String> subscribers = channels.get(channel);
        if (subscribers != null) {
            for (Integer connectionId : subscribers.keySet()) {
                ConnectionHandler<T> handler = clients.get(connectionId);
                if (handler != null) {
                    handler.send(msg);
                }
            }
        }
    }

    @Override
    public void disconnect(int connectionId) {
        clients.remove(connectionId);
        System.out.println("Connection with ID " + connectionId + " has been removed.");
    }

}
