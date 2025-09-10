# Web interface API

Put-In-Pipe documentation for front-end.

## Common endpoints

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
  "id": "2YrcZ1J9843a46tBnwF8UqqPkFEu5jEzu/irz2myTBE="
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

## Creating an identifier

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
  "captcha_image": "base64 encoded PNG"
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

The file transfer session is performed via WebSocket. This section describes the creation of such a session and the mechanism of joining.

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

## WebSocket connection

All websocket text messages contain JSON, which must have an "event" field.

```
/api/ws
```

To connect, it is necessary that the cookies contain a valid client ID and the client is attached to a valid session.

Immediately after connection, the server sends the information necessary to initialize the state, `start_init` server's event:

```
{
  "event": "start_init",
  {
    "session_id": "FpXC3S1Mf8nV8hhBWz/Qc9Fb3XEhFgKxaMJ9ZDDGXME=",
    "transferred": {
      "to_receivers": 0,
      "from_you": 0
    },
    "state": {
      "expiration_in": 7200,
      "some_chunk_was_removed": false,
      "chunk_queue": 0,
      "initial_freeze": true,
      "upload_finished": false,
      "current_chunk": 0
    },
    "members": {
      "sender": {
        "is_online": false,
        "name": "Data miner",
        "id": "FpXC3S1Mf8nV8hhBWz/Qc9Fb3XEhFgKxaMJ9ZDDGXME="
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

The summary object is shared by all users (for the sender and recipient) with one exception: `transferred` is shared only with the sender.

**Detailed description of fields**:

- `session_id` - Session ID (identical to the creator's public ID);
- `transferred` - The amount of data transferred in bytes (displayed only to the session creator).
- `state` - Global session status
  - `expiration_in` - The number of seconds remaining before the session is deleted due to reaching the maximum lifetime (timeout); 
  - `some_chunk_was_removed` - Whether at least one chunk was deleted. If true, new users cannot join such a session because some of the data is lost;
  - `chunk_queue` - Number of chunks in the buffer;
  - `initial_freeze` - Initial freezing. If active, chunks are not deleted from the buffer even if all known recipients have downloaded it. This allows a new recipient to connect to the session;
  - `upload_finished` - The sender uploaded the file in full;
  - `current_chunk` -  The number of the last uploaded chunk.
- `members` - List of participants
  - `sender` - Sender. One user;
  - `receivers` - An array containing other users with similar fields + `current_chunk` which shows their last downloaded chunk.
- `limits` - Global limits
  - `max_chunk_queue` - Maximum number of chunks in a buffer;
  - `max_initial_freeze` - The maximum duration of the initial freeze in seconds (when reached, the freeze will be reset automatically). **This parameter is also responsible for the maximum waiting time for recipients. If there are none, the session will be terminated**;
  - `max_chunk_size` - The maximum size of the chunk that the sender can upload (there is an excess of 16 bytes, which are the Poly1305 tag);
  - `max_receiver_count` - Maximum number of receivers.
