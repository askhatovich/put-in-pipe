## Web interface API

Put-In-Pipe documentation for front-end.

Put-In-Pipe - application for direct file transfer between users, where the server does not store the entire file (and therefore does not limit the size of the transferred file to its disk), but acts as an intermediate buffer: it accepted part of the file from the sender, gave it to the recipients, and so on until the file is transferred in its entirety. The architecture is based on the REST API and WebSocket, the implementation can be either a web application in JS or in other languages for desktop use. The server implementation leaves room for the frontend in the key to protecting the transmitted data: you can use end-to-end encryption between users (taking into account the size of the added overhead), or transfer files in the clear. At the beginning of the project, it was assumed that the transmitted files should be protected from interception on the ChaCha20-Poly1305 server side by placing the key in the link via an anchor (#), which, according to the HTTP standard, is not transmitted from the address bar to the server when opening the site.

# Common endpoints

A simple piece of logic, not directly related to file transfer

### Current workload indicators

When drawing the interface, it will be nice to indicate live data about the current server load. Obtaining this data does not require complex operations on the server side, so access to it is provided without authorization.

```
GET /api/statistics/current
```

The response is always with the `200` code. Payload example:

```
{
  "max_session_count": 300,
  "max_user_count": 100,
  "current_session_count": 1,
  "current_user_count": 2
}
```

### Identification information

Allows you to get information about the current authorization, or to understand that authorization is missing. Authorization allows you to join only one session and is reset when the session is deleted.

```
GET /api/me/info
```

Possible answers:

| Code | Body | Means |
|---|---|---|
| 401 | You have not been identified |  |
| 200 | **Read below** | The server successfully recognized the user and returned the information about him |

```
{
  "session": "",
  "name": "Data miner",
  "id": "2YrcZ1J9843a46tBnwF8UqqPkFEu5jEzuairz2myTBE"
}
```

If the `session` is empty, it means that the user has not joined any session.

### Reset authorization

```
POST /api/me/leave
```

Possible answers:

| Code | Body | Means |
|---|---|---|
| 401 | You have not been identified |  |
| 200 | Your ID has been deleted | The user is completely removed from the system |

---

# Creating an identifier

The service does not provide for the usual registration with a username and password. All users remain incognito, but to combat the harmful load and to ensure the operation of the application logic, the service installs cookies.

If the load is small, the service will install cookies without the active participation of the user. Otherwise, he will offer to solve the captcha.

For security reasons, HttpOnly cookie is used, which is inaccessible from the JS context.

After creating a user, it will be deleted in a minute if there is no websocket connection.

### Identification request

```
GET /api/identity/request?name=Data miner
```

For the convenience of interaction, the user needs to specify an arbitrary `name`. It will be displayed to other users within the same file transfer session.

Possible answers:

| Code | Body | Means |
|---|---|---|
| 400 | You are already authorized | The corresponding identifier from the cookie has been read. |
| 503 | The maximum number of clients has been reached, please try again later | The service has reached the maximum number of users installed in the configuration file. You can repeat the request after a few seconds, which can be considered as placing a new user in the queue. |
| 401 | **Read below** | It is necessary to solve the captcha. |
| 201 | `{"name": "Applied name", "id": "Public ID"}` | The ID was issued, and the response also contains a header for setting cookies. |

In the case where the response code is 401, it is necessary to display the captcha to the user. Server response structure:

```
{
  "client_id": "Ef1rei8YGRDhdld2NNnSTEpG",
  "captcha_lifetime": 180,
  "captcha_token": "some loooong string",
  "captcha_image": "base64 encoded PNG",
  "captcha_answer_length": 5
}
```

`captcha_lifetime` - captcha expiration date in seconds. It must be displayed to the user, for example, so that he can request a new captcha if he is distracted and does not have time.

### Identification confirmation

```
POST /api/identity/confirmation

{
  "name": "Data miner",
  "client_id": "value-from-the-server-response",
  "captcha_token": "value-from-the-server-response", 
  "captcha_answer": "user's input"
}
```

`name` - The username that he specified when opening the interface.

Possible answers:

| Code | Body | Means |
|---|---|---|
| 400 | *Some error text* | Notifies about an incorrect request signature, or that a cookie with a valid ID has already been found in the request. |
| 403 | *Some error text* | Notifies that the response to the captcha is incorrect, or the captcha has already been used and the user with the candidate ID already exists in the system. |
| 201 | `{"name": "Applied name", "id": "Public ID"}` | The ID was issued, and the response also contains a header for setting cookies. |

---

## Preparatory work for the transfer

This section describes the creation of such a session and the mechanism of joining.

### Create

```
POST /api/session/create

(empty body)
```

Possible answers:

| Code | Body | Means |
|---|---|---|
| 401 | You have not been identified | The cookie was not found, or it contains an invalid identifier. You need to start the process of obtaining the ID. |
| 403 | You are already a participant in the session | The user is already a participant in *some* session |
| 503 | The maximum number of sessions has been reached, please try again later | The service has reached the maximum number of sessions specified in the configuration file. You can repeat the request after a few seconds, which can be interpreted as adding a new session to the queue. **The timer for automatically deleting a user due to an unconnected websocket is reset with each such request.** |
| 500 | Session creation failed | Most likely, there was an error in the internal logic of the server. |
| 201 | `{"id": "SESSION_ID"}` | The session has been created |

`SESSION_ID` uses secure characters for the URL, so that it can be transmitted in the link without the risk of breaking the logic.

If successful, you need to establish a websocket connection.

### Join

```
GET /api/session/join?id=SESSION_ID
```

Possible answers:

| Code | Body | Means |
|---|---|---|
| 401 | You have not been identified | |
| 400 | *Error text*| Invalid request |
| 404 | Session not found | Invalid ID or the session was deleted |
| 403 | It is impossible to join the session | The session cannot accept a new user because the limit on the number of receivers has been reached |
| 202 | `{"id": "SESSION_ID"}` | Connection to the session was successful |

If successful, you need to establish a websocket connection.

---

## Downloading and uploading files

An alternative transmission method is via websocket, but in the case of websocket, transmission blocks the receipt of other events. 
It is recommended to use these methods.

### Get chunk

Request to download the chunk.

```
GET /api/session/chunk?id=NUMBER
```

| Code | Body | Means |
|---|---|---|
| 401 | You have not been identified | |
| 400 | *Error text*| Invalid request |
| 404 | Chunk not found | |
| 200 | *Binary data* | Requested binary data |

### Upload chunk

Request to add a new chunk to the buffer (only for the session creator).

```
POST /api/session/chunk
```

| Code | Body | Means |
|---|---|---|
| 401 | You have not been identified | |
| 400 | *Error text*| Invalid request |
| 403 | Only the session creator can send data | |
| 421 | Adding a chunk failed | The request was made at the wrong time: it is most likely that the buffer is full. |
| 500 | Session not found | |
| 202 | *Empty* | The data has been accepted and the new chunk has been successfully added |

---

# WebSocket connection

```
/api/ws
```

To connect, it is necessary that the cookies contain a valid client ID and the client is attached to a valid session. 
Only one active connection per user is allowed (when reconnecting, the previous websocket connection will be closed).

Immediately after connection, the server sends the information necessary to initialize the state, `start_init` server's event:

```
{
  "event": "start_init",
  "data": {
    "session_id": "FpXC3S1Mf8nV8hhBWzaQc9Fb3XEhFgKxaMJ9ZDDGXME",
    "transferred": {
      "global": {
        "to_receivers": 0,
        "from_sender": 0
      },
      "received_by_you": 0
    },
    "state": {
      "file": {
        "name": "",
        "size": 0
      }
      "expiration_in": 7200,
      "some_chunk_was_removed": false,
      "chunks": [
        {
          "index": 1,
          "size": 2356
        }
      ],
      "initial_freeze": true,
      "upload_finished": false,
      "current_chunk": 0
    },
    "members": {
      "sender": {
        "is_online": false,
        "name": "Data miner",
        "id": "FpXC3S1Mf8nV8hhBWzaQc9Fb3XEhFgKxaMJ9ZDDGXME"
      },
      "receivers": []
    },
    "limits": {
      "max_chunk_queue": 10,
      "max_initial_freeze": 120,
      "max_chunk_size": 5242880,
      "max_receiver_count": 5
    }
  }
}
```

The summary object is shared by all users (for the sender and recipient).

If `.state.file` has an empty name and zero size, this is a sure sign that the file data has not yet been installed.

**Detailed description of fields**:

- `session_id` - Session ID (identical to the creator's public ID);
- `transferred` - The amount of data transferred in bytes.
- `state` - Global session status
  - `expiration_in` - The number of seconds remaining before the session is deleted due to reaching the maximum lifetime (timeout); 
  - `some_chunk_was_removed` - Whether at least one chunk was deleted. If true, new users cannot join such a session because some of the data is lost;
  - `chunks` - Information about existing chunks (there are no chunks yet when creating the session, given as an example);
  - `initial_freeze` - Initial freezing. If active, chunks are not deleted from the buffer even if all known recipients have downloaded it. This allows a new recipient to connect to the session;
  - `upload_finished` - The sender uploaded the file in full;
  - `current_chunk` -  The number of the last uploaded chunk.
- `members` - List of participants
  - `sender` - Sender. It **can be set to `null`** if the user is disconnected (but the file is fully uploaded and the session continues to exist);
  - `receivers` - An array containing other users with similar fields + `current_chunk` which shows their last downloaded chunk.
- `limits` - Global limits
  - `max_chunk_queue` - Maximum number of chunks in a buffer;
  - `max_initial_freeze` - The maximum duration of the initial freeze in seconds (when reached, the freeze will be reset automatically). **This parameter is also responsible for the maximum waiting time for recipients. If there are none, the session will be terminated. If the file information is not set at the time the initial freeze is dropped, the session will also be destroyed**;
  - `max_chunk_size` - The maximum size of the chunk that the sender can upload;
  - `max_receiver_count` - Maximum number of receivers.
  
---

All websocket text messages contain JSON. Server messages must be processed using the `event` field, and all incoming messages from the client must contain the `action` key. The payload must always be contained in the `data` object. If the message grossly violates the expected format, i.e. violates the protocol described here, the connection is closed with a text description of the problem and code 1003 (Unsupported Data).

---

## Events sent by the server

### E1. Related to other users

#### E1.1 Online

Notifies of an active websocket connection. If the user is not connected for a long time (60 seconds), he is deleted.

```
{
  "event": "online",
  "data": {
    "id": "user's id",
    "status": true
  }
}
```

#### E1.2 Name change

```
{
  "event": "name_changed",
  "data": {
    "id": "user's id",
    "name": "New user's name"
  }
}
```

### E2. Related to the transfer session

#### E2.1 A new receiver in the session

```
{
  "event": "new_receiver",
  "data": {
    "id": "user's id",
    "name": "Username"
  }
}
```

#### E2.2 The client has been deleted

```
{
  "event": "receiver_removed",
  "data": {
    "id": "user's id"
  }
}
```

#### E2.3 The file information is set

```
{
  "event": "file_info",
  "data": {
    "name": "archive.zip",
    "size": 61359353
  }
}
```

Size in bytes.

#### E2.4 A new chunk is available

A new chunk has been uploaded, which is available for download.

```
{
  "event": "new_chunk",
  "data": {
    "index": 31,
    "size": 1536282
  }
}
```

Size in bytes.

#### E2.5 Downloading a chunk

The event indicates the start of the download and the finish when the receiver explicitly confirms receiving.

```
{
  "event": "chunk_download",
  "data": {
    "id": "user's id",
    "index": 31,
    "action": "started" or "finished"
  }
}
```

#### E2.6 The chunk has been deleted

```
{
  "event": "chunk_removed",
  "data": {
    "id": [
      78
    ]
  }
}
```

#### E2.7 Сounter of total transfered data has increased

```
{
  "event": "bytes_count",
  "data": {
    "value": 3927538,
    "direction": "from_sender" or "to_receivers"
  }
}
```

#### E2.8 The amount of data received

```
{
  "event": "personal_received",
  "data": {
    "bytes": 329273538
  }
}
```

#### E2.9 The initial freezing of chunks has been reset

```
{
  "event": "chunks_unfrozen",
  "data": {}
}
```

#### E2.10 The file upload is completed

After this event, new chunks cannot be created.

```
{
  "event": "upload_finished",
  "data": {}
}
```

#### E2.11 The session is over

```
{
  "event": "complete",
  "data": {
    "status": "Read below"
  }
}
```

Possible statuses:
- `ok` - Normal completion;
- `timeout` - The session was deleted due to timeout;
- `sender_is_gone` - The sender left the line;
- `no_receivers` - The receivers are gone (or never connected);
- `error` - Another error.

#### E2.12 The option to upload a new chunk is available

The event is only for the session creator. Notifies that the new chunk is available for upload. If it is not allowed, it means that the buffer is full.

```
{
  "event": "new_chunk_allowed",
  "data": {
    "status": false
  }
}
```

---
  
## Actions sent by the user

### A1. Admin options for the session creator

#### A1.1 Setting the file information

It can be specified only once. The size is specified in bytes.

```
{
  "action": "set_file_info",
  "data": {
    "name": "my_video.mp4",
    "size": 17654927 
  }
}
```

An error is possible if the information has already been set, or the size is zero, or an empty name has been passed, or the length of the name exceeds the limit (255 bytes). In case of an error, it returns an individual event:

```
{
  "event": "set_file_info_failure",
  "data": {}
}
```

#### A1.2 File upload completed

```
{
  "action": "upload_finished",
  "data": {}
}
```

#### A1.3 Delete the receiver

```
{
  "action": "kick_receiver",
  "data": {
    "id": "another-user-id"
  }
}
```

The session creator cannot delete himself, it will be an error.

#### A1.4 Forced session termination

```
{
  "action": "terminate_session",
  "data": {}
}
```

#### A1.5 Sending a new chunk

The session creator can send binary data. They will be treated as a new chunk and added to the session buffer. If adding to the buffer failed, an individual event will be returned:

```
{
  "event": "add_chunk_failure",
  "data": {}
}
```

An error is possible if the chunk buffer is full.

### A2. General actions

#### A2.1 Name change

```
{
  "action": "new_name",
  "data": {
    "name": "The lazy snail"
  }
}
```

The name must be in UTF-8 format. The length of the name is automatically shortened to the maximum allowed (20 characters).

#### A2.2 Getting a chunk

```
{
  "action": "get_chunk",
  "data": {
    "index": 78
  }
}
```

If the requested fragment is not found, a separate event is returned containing information about existing  chunks:

```
{
  "event": "requested_chunk_not_found",
  "data": {
    "available": [
      {
        "index": 79,
        "size": 62857
      },
      {
        "index": 80,
        "size": 62857
      }
  ]
}
```

If successful, the response contains the binary of the requested chunk.

#### A2.3 Confirmation of receiving of the chunk

A chunk is considered received only after explicit confirmation. When a chunk is received by all receivers, it is deleted from the buffer, making room for a new chunk.

```
{
  "action": "confirm_chunk",
  "data": {
    "index": 81
  }
}
```
