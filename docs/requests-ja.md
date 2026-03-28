# Request & Response

基本形

- JSON 成功応答: `{ success: true, message: "OK" | "Created", ... }`
- JSON 失敗応答: `{ success: false, code, message, details? }`
- RAW 応答: `GET /` などでは `text/plain` または file body をそのまま返す
- `DELETE` 成功時は `204 No Content`

## GET /

#### request:

```http
GET / HTTP/1.1
Host: localhost:8080
```

#### response:

```text
hello world
```

## GET /?id=8&json=true

#### request:

```http
GET /?id=8&json=true HTTP/1.1
Host: localhost:8080
Accept: application/json
```

#### response:

```json
{
  "success": true,
  "message": "OK",
  "data": [
    {
      "id": 8,
      "is_file": false,
      "content": "hello world",
      "created_at": 1711111111,
      "updated_at": 1711112222
    }
  ]
}
```

## POST / ('application/json')

#### request:

```http
POST / HTTP/1.1
Host: localhost:8080
Content-Type: application/json

{"content":"new text"}
```

#### response:

```json
{
  "success": true,
  "message": "Created",
  "id": 9
}
```

## PUT /?id=9

#### request:

```http
PUT /?id=9 HTTP/1.1
Host: localhost:8080
Content-Type: application/json

{"content":"replaced text"}
```

#### response:

```json
{
  "success": true,
  "message": "OK",
  "id": 9
}
```

## PATCH /?id=9

#### request:

```http
PATCH /?id=9 HTTP/1.1
Host: localhost:8080
Content-Type: application/json

{"content":"patched text"}
```

#### response:

```json
{
  "success": true,
  "message": "OK",
  "id": 9
}
```

## POST /swap?id1=3&id2=4

#### request:

```http
POST /swap?id1=3&id2=4 HTTP/1.1
Host: localhost:8080
```

#### response:

```json
{
  "success": true,
  "message": "OK",
  "data": [
    {
      "id": 3,
      "is_file": false,
      "filename": "note.txt",
      "mime": "text/plain",
      "created_at": 1711113333,
      "updated_at": 1711113333
    },
    {
      "id": 4,
      "is_file": false,
      "content": "slot four text",
      "created_at": 1711112222,
      "updated_at": 1711112222
    }
  ]
}
```

## DELETE /?id=9

#### request:

```http
DELETE /?id=9 HTTP/1.1
Host: localhost:8080
```

#### response:

```text
HTTP/1.1 204 No Content
```

## GET /search?q=note&limit=2&type=text&sort=id&order=desc

#### request:

```http
GET /search?q=note&limit=2&type=text&sort=id&order=desc HTTP/1.1
Host: localhost:8080
Accept: application/json
```

#### response:

```json
{
  "success": true,
  "message": "OK",
  "data": [
    {
      "id": 12,
      "is_file": false,
      "content": "alpha note",
      "created_at": 1711111111
    },
    {
      "id": 10,
      "is_file": false,
      "content": "beta note",
      "created_at": 1711110000
    }
  ],
  "meta": {
    "count": 2,
    "limit": 2,
    "sort": "id",
    "order": "desc"
  }
}
```

## GET /search?limit=3&sort=stored_at&order=desc

#### request:

```http
GET /search?limit=3&sort=stored_at&order=desc HTTP/1.1
Host: localhost:8080
Accept: application/json
```

#### response:

```json
{
  "success": true,
  "message": "OK",
  "data": [
    {
      "id": 12,
      "is_file": false,
      "content": "alpha note",
      "created_at": 1711111111
    },
    {
      "id": 11,
      "is_file": true,
      "filename": "sound.mp3",
      "mime": "audio/mpeg",
      "created_at": 1711111000
    },
    {
      "id": 10,
      "is_file": false,
      "content": "beta note",
      "created_at": 1711110000
    }
  ],
  "meta": {
    "count": 3,
    "limit": 3,
    "total": 12,
    "sort": "stored_at",
    "order": "desc"
  }
}
```

## GET /search/live?q=alp&limit=5

#### request:

```http
GET /search/live?q=alp&limit=5 HTTP/1.1
Host: localhost:8080
Accept: application/json
```

#### response:

```json
{
  "success": true,
  "message": "OK",
  "data": [
    {
      "id": 12,
      "is_file": false,
      "preview": "alpha note",
      "created_at": 1711111111
    },
    {
      "id": 11,
      "is_file": false,
      "preview": "alpine memo",
      "created_at": 1711111000
    }
  ],
  "meta": {
    "count": 2,
    "limit": 5,
    "sort": "id",
    "order": "desc",
    "live": true
  }
}
```

## GET /health

#### request:

```http
GET /health HTTP/1.1
Host: localhost:8080
Accept: application/json
```

#### response:

```json
{
  "status": "ok",
  "version": "0.0.0",
  "limit": 100,
  "size": {
    "file": "10MB",
    "text": "1MB"
  },
  "path": {
    "db": "/var/lib/karing/karing.sqlite",
    "upload": "/var/lib/karing/uploads",
    "log": "/home/user/.local/state/karing/logs"
  },
  "listener": {
    "address": "0.0.0.0",
    "port": 8080
  },
  "db": {
    "active_items": 3,
    "max_items": 100,
    "next_id": 4
  }
}
```
