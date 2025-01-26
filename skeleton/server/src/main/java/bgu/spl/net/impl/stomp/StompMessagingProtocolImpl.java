package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.impl.rci.Command;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;
import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;


public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {
 //fileds
 private int connectionId;
 private Connections<String> connections;
 private boolean shouldTerminate;

 public static ConcurrentHashMap<String, String> registeredUsers = new ConcurrentHashMap<>();
 private static ConcurrentHashMap<String, ConcurrentHashMap<Integer, String>> subscriptions = new ConcurrentHashMap<>();
 private static ConcurrentHashMap<String, Integer> activeConnections = new ConcurrentHashMap<>();


 //functions
 @Override
 public void start(int connectionId, Connections<String> connections) {
     this.connectionId = connectionId;
     this.connections = connections;
     this.shouldTerminate = false;
 }

 @Override
 public boolean shouldTerminate() {
     return shouldTerminate;
 }
 @Override
 public String process(String message) {
     String[] lines = message.split("\n");
     String command = lines[0];

     System.out.println("[DEBUG] input is :" + message);

     switch (command) {
         case "CONNECT":
             return processConnect(lines);
             
         case "SEND":
             return processSend(lines);
             
         case "SUBSCRIBE":
             return processSubscribe(lines);
             
         case "UNSUBSCRIBE":
             return processUnsubscribe(lines);
             
         case "DISCONNECT":
             processDisconnect(lines);
             break;
         default:
             handleError("Unknown command: " + command, null);
             break;
     }
     return null;
 }

 // process commands impl
 private String processConnect(String[] lines) {
     String username = null;
     String password = null;
     String version = null;

     System.out.println("[DEBUG] Received CONNECT frame:");
     
     for (String line : lines) {
         if (line.startsWith("login:")) {
             username = line.substring(6);
             System.out.println("[DEBUG] username is:" + username);
         } else if (line.startsWith("passcode:")) {
             password = line.substring(9);
             System.out.println("[DEBUG] password is:" + password);
         } else if (line.startsWith("accept-version:")) {
             version = line.substring(15).trim();  // חיפוש גרסה מתוך ההודעה
             System.out.println("[DEBUG] accept-version is: " + version);
        }
     }

     if (registeredUsers.containsKey(username)) {
         if (!registeredUsers.get(username).equals(password)) {
             handleError("Wrong password", null);
             return null;
         }
         if (activeConnections.containsKey(username)) {
             handleError("User already logged in", null);
             return null;
         }
     } else {
         registeredUsers.put(username, password);
     }
     activeConnections.put(username, connectionId);

    StringBuilder builder = new StringBuilder();
    builder.append("CONNECTED");
    builder.append("\n");
    builder.append("version:");
    builder.append(version);
    builder.append("\n");
    builder.append("\n");

    String connectedFrame = builder.toString();
    
    connections.send(connectionId, connectedFrame);
    return null;
    }

 private String processSubscribe(String[] lines) {
     String destination = null;
     String subscriptionId = null;
     String receiptId = null;

     for (String line : lines) {
        if (line.startsWith("destination:")) {
            destination = line.substring(12);

            if (destination.startsWith("/")) {
                destination = destination.substring(1);
             }
             if (destination.endsWith("/")) {
                destination = destination.substring(0, destination.length() - 1); 
             }
        } else if (line.startsWith("id:")) {
            subscriptionId = line.substring(3).trim(); 
        } else if (line.startsWith("receipt:")) {
            receiptId = line.substring(8).trim(); 
        }
    }

    System.out.println("[DEBUG] Subscribe - destination: " + destination + ", id: " + subscriptionId);

     if (destination == null || subscriptionId == null || receiptId == null) {
         handleError("[DEBUG] Missing destination or id header or recipt id ", receiptId);
         return null;
     }

     connections.subscribe(destination, connectionId, subscriptionId);


     //subscriptions.putIfAbsent(destination, new ConcurrentHashMap<>());
     //subscriptions.get(destination).put(connectionId, subscriptionId);

     System.out.println("[DEBUG] After adding subscriber:");
     System.out.println("[DEBUG] Subscriptions map: " + subscriptions);
     System.out.println("[DEBUG] Active connections map: " + activeConnections);

    
     /*StringBuilder builder = new StringBuilder();
     builder.append("RECEIPT");
     builder.append("\n");
     builder.append("receipt-id:");
     builder.append(receiptId);
     builder.append("\n");
     builder.append("\n");*/

     StringBuilder builder = new StringBuilder();
     builder.append("RECEIPT\n");
     builder.append("receipt-id:").append(receiptId).append("\n\n");

     System.out.println("Joined channel " + destination);
     String receipt = builder.toString();
     connections.send(connectionId, receipt);
    
     return receipt;

 }


private String processSend(String[] lines) {
    String destination = null;
    
    // Extract destination
    for (String line : lines) {
        if (line.startsWith("destination:/")) {
            destination = line.substring(12);
            if (destination.startsWith("/")) {
                destination = destination.substring(1);
             }
             if (destination.endsWith("/")) {
                destination = destination.substring(0, destination.length() - 1); 
             }
            break;
        }
    }

    if (destination == null) {
        handleError("Missing destination header", null);
        return null;
    }

    // Check subscription
    if (!((ConnectionsImpl<String>)connections).isUserSubscribedToTopic(connectionId, destination)) {
        handleError("Not subscribed to topic: " + destination, null);
        return null;
    }

    // Convert to MESSAGE frame
    String messageFrame = "MESSAGE\n" + String.join("\n", lines).replace("SEND\n", "");
    connections.send(destination, messageFrame);
    return null;
}
  
/*private String processUnsubscribe(String[] lines) {
    String subscriptionId = null;
    String receiptId = null;

    for (String line : lines) {
        if (line.startsWith("id:")) {
            subscriptionId = line.substring(3);
        } else if (line.startsWith("receipt:")) {
         receiptId = line.substring(8);
        }
    }

     boolean removed = false;

     for (String destination : subscriptions.keySet()) {
        ConcurrentHashMap<Integer, String> subscribers = subscriptions.get(destination);

        if (subscribers.containsKey(connectionId) && subscribers.get(connectionId).equals(subscriptionId)) {
            subscribers.remove(connectionId);
            removed = true;
            System.out.println("Exited channel " + destination);
            break;
        }

        if (!removed) {
            handleError("Subscription ID not found: " + subscriptionId, receiptId);
            return null;
        }
     }

    StringBuilder builder = new StringBuilder();
        builder.append("RECEIPT");
        builder.append("\n");
        builder.append("receipt-id:");
        builder.append(receiptId);
        builder.append("\n");
        builder.append("\n");

    String connectedFrame = builder.toString();
    connections.send(connectionId, connectedFrame);

    return connectedFrame;

 }*/

 private String processUnsubscribe(String[] lines) {
    String subscriptionId = null;
    String receiptId = null;

    // שליפת subscriptionId ו־receiptId מתוך השורות
    for (String line : lines) {
        if (line.startsWith("id:")) {
            subscriptionId = line.substring(3).trim();
        } else if (line.startsWith("receipt:")) {
            receiptId = line.substring(8).trim();
        }
    }

    if (subscriptionId == null || receiptId == null) {
        handleError("Missing subscription ID or receipt header", receiptId);
        return null;
    }

    // הסרת המנוי באמצעות connections.unsubscribe
    boolean unsubscribed = ((ConnectionsImpl<String>) connections).unsubscribe(subscriptionId, connectionId);

    if (!unsubscribed) {
        handleError("Subscription ID not found: " + subscriptionId, receiptId);
        return null;
    }

    StringBuilder builder = new StringBuilder();
    builder.append("RECEIPT");
    builder.append("\n");
    builder.append("receipt-id:");
    builder.append(receiptId);
    builder.append("\n");
    builder.append("\n");

    String receiptFrame = builder.toString();

    connections.send(connectionId, receiptFrame);

    return receiptFrame;
}


private void handleError(String errorMessage, String receiptId) {

     StringBuilder errorFrame = new StringBuilder();

     errorFrame.append("ERROR\n");

     errorFrame.append("message:").append(errorMessage).append("\n");

     if (receiptId != null && !receiptId.isEmpty()) {
     errorFrame.append("receipt-id:").append(receiptId).append("\n");
    }

     errorFrame.append("\n\u0000");
     connections.send(connectionId, errorFrame.toString());
     shouldTerminate = true;
 }  

 private void processDisconnect(String[] lines) {
    String receiptId = null;
    for (String line : lines) {
        if (line.startsWith("receipt:")) {
            receiptId = line.substring(8).trim();
            break;
        }
    }

    if (receiptId == null) {
        handleError("Missing receipt header", null);
    }

    // Send receipt before disconnecting
    String receipt = String.format("RECEIPT\nreceipt-id:%s\n\n", receiptId);
    connections.send(connectionId, receipt);
    
    // Disconnect client
    connections.disconnect(connectionId);
    shouldTerminate = true;
    
}
}
