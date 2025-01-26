package bgu.spl.net.srv;

import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

public class ConnectionsImpl<T> implements Connections<T> {

    /*
     * a map that connect between connectionId and ConnectionHandler<T>
     * (object that represent the connection between server aand client)
     */
    private ConcurrentHashMap<Integer, ConnectionHandler<T>> clients;
    private final ConcurrentHashMap<String, Set<Integer>> channels;
    private ConcurrentHashMap<String, String> registeredUsers;
    private ConcurrentHashMap<String, Integer> activeConnections;

    public ConnectionsImpl() {
        this.clients = new ConcurrentHashMap<>();
        this.channels = new ConcurrentHashMap<>();
        this.registeredUsers = new ConcurrentHashMap<>();
        this.activeConnections = new ConcurrentHashMap<>();
        System.out.println("[DEBUG][ConnectionsImpl] Initialization complete");
    }
    
    
    @Override
    public boolean send(int connectionId, T msg) {
        System.out.println("[DEBUG] Im in send (ConnectionsImpl) with connID:"+ connectionId + "msg:"+ msg);
        ConnectionHandler<T> handler = clients.get(connectionId);
        if (handler != null) {
            System.out.println("[DEBUG][ConnectionsImpl] Handler found, sending message");
            handler.send(msg);
            return true;
        }
        System.out.println("[DEBUG][ConnectionsImpl] No handler found for connection " + connectionId);
        return false;
    }

   /*@Override
    public void send(String channel, T msg) {
        System.out.println("[DEBUG][ConnectionsImpl] Broadcasting to channel: " + channel);
        ConcurrentHashMap<Integer, String> subscribers = channels.get(channel);
        if (subscribers != null) {
            System.out.println("[DEBUG][ConnectionsImpl] Found " + subscribers.size() + " subscribers");
            for (Integer connectionId : subscribers.keySet()) {
                System.out.println("[DEBUG][ConnectionsImpl] Sending to subscriber: " + connectionId);
                ConnectionHandler<T> handler = clients.get(connectionId);
                if (handler != null) {
                    handler.send(msg);
                }
            }
        }
    }
*/

    @Override
    public void send(String channel, T msg) {
        Set<Integer> subscribers = channels.get(channel);
        if (subscribers != null) {
            for (Integer connectionId : subscribers) {
                send(connectionId, msg);
            }
        }
    }

     
    /*@Override
    public void disconnect(int connectionId) {
        clients.remove(connectionId);
        System.out.println("[DEBUG] Connection with ID " + connectionId + " has been removed.");

         // Remove from all topics
         for (String topic : channels.keySet()) {
            if (channels.get(topic).remove(connectionId) != null) {
                System.out.println("[DEBUG][ConnectionsImpl] Removed from topic: " + topic);
            }
        }
        
        // Remove from active connections
        String username = null;
        for (String user : activeConnections.keySet()) {
            if (activeConnections.get(user) == connectionId) {
                username = user;
                break;
            }
        }
        if (username != null) {
            System.out.println("[DEBUG][ConnectionsImpl] Removing active user: " + username);
            activeConnections.remove(username);
        }
        
        System.out.println("[DEBUG][ConnectionsImpl] Disconnect complete for client " + connectionId);
    }*/

    @Override
    public void disconnect(int connectionId) {
        // Remove client
        clients.remove(connectionId);
        System.out.println("[DEBUG] Connection with ID " + connectionId + " has been removed.");

        ConcurrentHashMap<Integer, Boolean> pendingFrames = new ConcurrentHashMap<>();

        // Remove from topics
        for (String topic : channels.keySet()) {
            if (channels.get(topic).contains(connectionId)) {
                channels.get(topic).remove(connectionId);
                System.out.println("[DEBUG][ConnectionsImpl] Removed from topic: " + topic);
            }
        }

        // Remove from active connections
        String username = activeConnections.entrySet()
            .stream()
            .filter(entry -> entry.getValue() == connectionId)
            .map(Map.Entry::getKey)
            .findFirst()
            .orElse(null);

        if (username != null) {
            System.out.println("[DEBUG][ConnectionsImpl] Removing active user: " + username);
            activeConnections.remove(username);
        }

        System.out.println("[DEBUG][ConnectionsImpl] Disconnect complete for client " + connectionId);
    }

    public void addClient(int connectionId, ConnectionHandler<T> handler) {
        if(handler == null) {
            System.out.println("[ERROR] Null handler for connection " + connectionId);
            return;
        }
        clients.put(connectionId, handler);
        System.out.println("[DEBUG][ConnectionsImpl] Total clients: " + clients.size());
    }


    public boolean registerUser(String username, String password) {
        System.out.println("[DEBUG][ConnectionsImpl] Attempting to register user: " + username);
        if (!registeredUsers.containsKey(username)) {
            System.out.println("[DEBUG][ConnectionsImpl] New user registration");
            registeredUsers.put(username, password);
            return true;
        }
        boolean correctPassword = registeredUsers.get(username).equals(password);
        System.out.println("[DEBUG][ConnectionsImpl] Existing user login attempt - Password correct: " + correctPassword);
        return correctPassword;
    }

    public boolean isUserActive(String username) {
        boolean isActive = activeConnections.containsKey(username);
        System.out.println("[DEBUG][ConnectionsImpl] Checking if user " + username + " is active: " + isActive);
        return isActive;
    }

    public void activateUser(String username, int connectionId) {
        System.out.println("[DEBUG][ConnectionsImpl] Activating user " + username + " with connection " + connectionId);
        activeConnections.put(username, connectionId);
    }

    /*public boolean subscribe(String topic, int connectionId, String subscriptionId) {
        System.out.println("[DEBUG][ConnectionsImpl] Subscribing connection " + connectionId + " to topic " + topic);
        channels.putIfAbsent(topic, new ConcurrentHashMap<>());
        channels.get(topic).put(connectionId, subscriptionId);
        System.out.println("[DEBUG][ConnectionsImpl] Subscription successful");
        return true;
    }*/

    /*public boolean unsubscribe(String topic, int connectionId) {
        System.out.println("[DEBUG][ConnectionsImpl] Unsubscribing connection " + connectionId + " from topic " + topic);
        if (channels.containsKey(topic)) {
            String subscriptionId = channels.get(topic).get(connectionId);
            boolean removed = channels.get(topic).remove(connectionId) != null;
            System.out.println("[DEBUG][ConnectionsImpl] Unsubscribe " + (removed ? "successful" : "failed"));
            
            // Return the subscription ID that was removed
            return subscriptionId != null;
        }
        System.out.println("[DEBUG][ConnectionsImpl] Topic not found");
        return false;
    }*/

    @Override
    public boolean subscribe(String topic, int connectionId, String subscriptionId) {
        try {
            System.out.println("[DEBUG][Subscribe] Attempting to subscribe connectionId " + connectionId + " to topic: " + topic);
            
            // Initialize topic if doesn't exist
            channels.putIfAbsent(topic, ConcurrentHashMap.newKeySet());
            
            // Check if already subscribed
            if (channels.get(topic).contains(connectionId)) {
                System.out.println("[DEBUG][Subscribe] Already subscribed to topic " + topic);
                return false;
            }
            
            // Add subscription
            boolean success = channels.get(topic).add(connectionId);
            System.out.println("[DEBUG][Subscribe] Subscription status: " + success);
            return success;
            
        } catch (Exception e) {
            System.err.println("[ERROR][Subscribe] Failed to subscribe: " + e.getMessage());
            e.printStackTrace();
            return false;
        }
    }

    @Override
    public boolean unsubscribe(String topic, int connectionId) {
        try {
            for (Map.Entry<String, Set<Integer>> entry : channels.entrySet()) {
                Set<Integer> subscribers = entry.getValue();
                if (subscribers.contains(connectionId)) {
                    boolean removed = subscribers.remove(connectionId);
                    System.out.println("[DEBUG][Unsubscribe] " + (removed ? "Success" : "Failed") + " for topic: " + entry.getKey());
                    return removed;
                }
            }
            return false;
        } catch (Exception e) {
            System.err.println("[ERROR][Unsubscribe] Failed: " + e.getMessage());
            return false;
        }
    }

    /*public boolean isUserSubscribedToTopic(int connectionId, String topic) {
        System.out.println("[DEBUG][ConnectionsImpl] Checking if connection " + connectionId + " is subscribed to " + topic);
        ConcurrentHashMap<Integer, String> subscribers = channels.get(topic);
        boolean isSubscribed = subscribers != null && subscribers.containsKey(connectionId);
        System.out.println("[DEBUG][ConnectionsImpl] Is subscribed: " + isSubscribed);
        return isSubscribed;
    }*/

    public boolean isUserSubscribedToTopic(int connectionId, String topic) {
        System.out.println("[DEBUG][ConnectionsImpl] Checking if connection " + connectionId + " is subscribed to " + topic);
    
        Set<Integer> subscribers = channels.get(topic);
    
        boolean isSubscribed = subscribers != null && subscribers.contains(connectionId);
    
        System.out.println("[DEBUG][ConnectionsImpl] Is subscribed: " + isSubscribed);
        return isSubscribed;
    }
}

