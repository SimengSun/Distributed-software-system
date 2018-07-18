# Distributed-software-system
A course project by Chenchen Hu, Simeng Sun, Yecheng Yang and Shifan Zhang.
The project will reappear every semester, thus codes are not made publically available.

## Load balancer
The front end coordinator/master servers as a load balancer for all incoming client request to our system. It finds the frontend server with fewest ongoing requests at the moment efficiently (log(n)) and redirect client to that server through HTTP redirect. The load balancer also receives ping from frontend nodes to determine their status. If a node missed a few consecutive pings, it will be marked as inactive. Upon reception of these pings, load balancer forwards the results to the backend master (more on this in the next subsection). 

**Dynamic Membership**: The load balancer starts with a static configuration file that contains information about frontend servers but can also accept dynamic members through specific commands.

## Backend server and master

**Replication and consistency:**
Primary based replication, sequential consistency: To ensure consistent and resilient service, we decided to construct our infrastructure with primary-based replication and remote write. Specifically, a primary node will try to execute the command locally and later propagate this command to all its active secondaries nodes. A secondary node will follow remote write protocol and forward all requests from the client to primary. If only all active nodes responsible for this tablet successfully executes the command and send a OK message, the primary will return OK message to the client. Through this replication scheme, we can assure data is successfully backed up to all replicas and in addition, we can achieve sequential consistency by propagating writes from the primary.
**Fault tolerance:**
*Periodically checkpoints:* Each of the backend servers will periodically write checkpoints to local big tables to reduce amount of data kept in the log. 

*Recovery through transferred log:* Backend nodes periodically send ping to backend master to indication that it is still active. Upon several consecutive missing pings, the master will mark backend node as inactive and notify nodes responsible for the tablets of the crashed node to keep additional logs. If the crashed node is primary, the master will ask one of the secondary nodes to become the new primary node. During reboot of a backend node A, it will first recover from local log and then ping the master to determine whether it is primary or secondary and whether it needs to recover from remote logs. If such recovery is necessary, node A will ask another node B for additional log. Node B will then send all additional logs it has been keeping in order and clear them. Node A will redo every single log transferred from B.

*Admin Console:* Admin console provides an interface which the administrator can check stats and address of each backend and frontend node. It also provides view of data on node as well as killing the node.

**Dynamic Membership:** The load balancer starts with a static configuration file that contains information about frontend servers but can also accept dynamic members through specific commands.

## Frontend server
Frontend server is responsible for the communication between the clients (Web Browser) and the backend server. It is able to recognize and handle three kinds of http requests sent by the web browser → GET / HEAD / POST. GET request is sent by the web browser to fetch certain static webpage (html) from the server. GET and HEAD requests are handled locally while POST request requires communication with backend server. POST is sent to fetch data or change data in the backend (e.g. change password request from browser) and thus would get dynamic webpage with user-specific information. For the post command, we use the ajax. When the browser loads the html page at the first time, it would send a request to the frontend server, front-end server handles the request, interacts with backend, return the corresponding data (JSON format) to browser, and the browser would automatically reform the web page based on the returned data.

Frontend server would generate a session id for each user. The session id would be sent to backed and saved into bigtable, each time the frontend server receives a request, it would firstly verify if the user has valid session id (cookie).  

## Email service
The email system provides four major operations: `send`, `reply`, `forward` and `delete`. In front end, after logging in, user can view all inbox email headers with sender, title and time. By clicking view button, user can see the content as well as do reply, forward and delete. We implement the backend mail service by adding an inbox column for each user after signing up. Each mail is saved as a separate column. The communication between frontend and backend is done by serializing mail object as byte array using google protobuf. 

Besides, We integrate smtp server as front end with our PennCloud system. We can send mail through telnet and view the mail in browser. 

## Key-value store
The big table provides put, cput, get and delete, with row locks for cput. To implement that,  we keep a lock map for bigtable, and a single mutex for the lock map. In memory, bigtable is implemented as map of map, each cell has its bytes array value, file type and unlimited size. For all operations, we record it in local binary log. For every 30 seconds, we do a checkpoint, which serializes big table as a binary file, truncate the log file and log the checkpoint. When a big table is initiated, it reads from disk file, roll back all unchecked log operations and initiate the locks.

## Web storage service
In general, the operation we support for files includes `upload`, `download`, `delete`, `rename` and `move`; the operation we support for folders includes `create`, `delete` and `move`. In addition, we also support the service for getting file/folder list the user currently have. To be more specific, we store **metadata info** for each folder. Such metadata is a list of **metadata item**, where each file or folder in the current folder corresponds to such a metadata item. The item includes information about whether it is a file or folder; if it is a file, it includes the column key for the file; If it is a folder, it includes the column key for the metadata information for this folder (inside current folder). The column key for a file is computed through the hash value of file content and has nothing to do with storage path. Thus when operates on file, we only need to change the metadata information. For instance, when we need to move a file from one folder to another, we first get metadata of both target folder and source folder, then delete metadata item from source folder metadata info and add new item to target metadata info, finally we send cput for both updated metadata info. On signing up a user, we automatically create root folder metadata info for this user, we heuristically name root directory './rootdir/'. We thus require all folder path start with ‘./rootdir/’ and end with ‘/’. 

## Account related service
For user account service, we support `login`, `signup` and `reset password`. When a user firstly sign up in the system, we store <username, password> and <username, ‘./root/dir’> to bigtable. When a user tries to login, frontend server will first get true password for this user, if the true password matches what user has input, frontend server will store <sessionID, username> to bigtable. The frontend server need to retrieve the username associated with a sessionID from bigtable before it can proceed with other operations since all proceeding operations rely on the username as row key.

