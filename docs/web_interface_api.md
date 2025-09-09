# Web interface API

Put-In-Pipe documentation for front-end.

## Creating an identifier

The service does not provide for the usual registration with a username and password. All users remain incognito, but to combat the harmful load and to ensure the operation of the application logic, the service installs cookies.

If the load is small, the service will install cookies without the active participation of the user. Otherwise, he will offer to solve the captcha.

For security reasons, HttpOnly captcha is used, which is inaccessible from the JS context.

### Identification request

```
GET /api/identity/request
```

Possible answers:

| Code | Body | Means |
|---|---|---|
| 400 | You are already authorized | The corresponding identifier from the cookie has been read. |
| 503 | The maximum number of clients has been reached, please try again later | The service has reached the maximum number of users installed in the configuration file. You can repeat the request after a few seconds, which can be considered as placing a new user in the queue. |
| 401 | **Read below** | It is necessary to solve the captcha. |
| 201 | *None* | The ID has been issued, and the response instructs the browser to set a cookie. |

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
  "client_id": "value-from-the-server-response",
  "captcha_token": "value-from-the-server-response", 
  "captcha_answer": "user's input"
}
```

| Code | Body | Means |
|---|---|---|
| 400 | *Some error text* | Notifies about an incorrect request signature, or that a cookie with a valid ID has already been found in the request. |
| 403 | *Some error text* | Notifies that the response to the captcha is incorrect, or the captcha has already been used and the user with the candidate ID already exists in the system. |
| 201 | *None* | The ID has been issued, and the response instructs the browser to set a cookie. |

---

## Preparatory work for the transfer session

The file transfer session is performed via WebSocket. This section describes the creation of such a session and the mechanism of joining.
