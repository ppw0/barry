# yoserv
network server in C

Based on https://www.youtube.com/watch?v=FNc_nWbSMM8

## Build
requires libsqlite3-dev
```bash
make
./yoserv.o
```
In a separate prompt, connect with
```bash
$ nc localhost 1234
```
## Usage
```C
Greeting MEIS
Send Yo  YOYO
Collect  YOLO
List all YOSR
Exit     NOYO
```
## yo.db schema
```SQL
CREATE TABLE users (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  uname TEXT NOT NULL UNIQUE
);

CREATE TABLE yos (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  yofrom TEXT REFERENCES users(uname),
  yoto TEXT REFERENCES users(uname)
);
```
## Clearing the database
```bash
$ sqlite3 yo.db
> delete from users;
> delete from yos;
```
## Notes from the lecture
* You cannot use an already opened socket.
* Even after closing the socket and the program quits, it takes a while (between 1 and 4 minutes) for the address to be freed as the socket is in `TIME_WAIT` state, waiting for any packets yet to arrive. This issue is mitigated by attempting to reuse the address even if the process has crashed or has been killed, by using setsockopt and `SO_REUSEADDR`. Alternative possible solutions are `SO_REUSEPORT` and setting `SO_LINGER`.
* After opening the socket, we need to bind it to the server address. `bind` expects a `struct sockaddr*`. We get a compiler error because `serv_addr` is a `sockaddr_in` and `bind` is expecting just a `sockaddr`, because there are many different types of sockets you can create and the internet one is just one of them.
* (man page) perror produces a message on stderr describing the last error encountered during a call to a system or library function.
* After we bind, we listen for a connection. This pauses the program and waits for a connection to come in. The '5' determines how many outstanding connections we will allow. While it's processing a connection, there can be 5 incoming connections in the queue waiting to be picked up.
* Then, we accept the connection (i.e. assign to ourselves a new socket number) so that we can accept new connections after this one. If a connection came in and used the same socket number that was being used to create the socket, you can only accept one connection, because you'd have only one communication channel going across that particular socket, so when you accept, you get a new socket number, and then the old one can be recycled and used again. After accepting, `accept` will give us the address of the client who connected to us, so we need another variable, `cli_addr`.
* What we want is the ability to accept multiple connections. The way to do that is to make it in a way, almost 'multithreaded'. The cheap way to do it is you get a connection and you do what's called a fork, so the process duplicates itself and then one process lives on to accept a new connection and the other process processes the query, processes the transaction, and then quits. After we bind the connection, we do a `listen`, and then we're going to go into a loop. So we're going to accept, and then cause the process to split into two.
* The function `do_yo` functions as a pseudo-state machine.
* The client sends a bunch of characters in an array in a buffer - they are not C strings and they are not null-terminated. We get a buffer back from the client and we know how many bytes we've received, so we're going to null-terminate it, and that way, if they didn't type anything, there's going to be a null sitting there, and when we run `scanf`, there'll be no characters to scan.
* If the buffer gets full, using `strncmp` ensures that the comparison will not run off the end of the buffer and start comparing data that's in memory. The 'n' in `strncmp` means that the string comparison will compare at most 'n' characters.
* `sscanf` returns -1 if the scanning terminated early, that is, if there was nothing to scan (we haven't specified a username). -1 has the same meaning as EOF.
* Every time someone logs in, we will insert their username into the table.
* There's a serious problem here, and that's inserting SQL statements into strings. This is the problem of doing database queries. Here, you're essentially doing what's called 'programming by printf'. You embed code in a string and then you send that string off to the database. The problem is we're not checking what the user types in %s, like any characters that the database might interpret as being part of the command. For example, if we typed a quote mark in the username, you would think that the string was stopping early and then the rest of the command was just an unknown syntax error, which would cause the SQL statement to fail. So there are SQL injection attacks, where you can carefully craft a string that will cause the database to do something it's not supposed to do, like maybe insert a user in there whose password you know, or get a normal user to log in as administrator. So the thing to do there is to do what's called either sanitize the string, which is to remove any special characters or at least escape them, or avoid doing this entirely.
* We cannot let users send messages to users that don't exist. Since the database does not enforce inserting a row for a user that doesn't exist, we have to check that the user exists ourselves by querying the database.

### sqlite3 notes:
* `sqlite3_preparev2()` prepares an SQL statement for execution.
* `sqlite3_step(stmt)` evaluates the statement.
* `sqlite3_finalize(stmt)` finalizes a prepared statement, which must be done to avoid resource leaks.
* `sqlite3_exec()` combines `sqlite3_preparev2()`, `sqlite3_step()` and `sqlite3_finalize()`.
* `while (sqlite3_step(stmt) == SQLITE_ROW)` evaluates the statement and indicates if there’s more data.
* `sqlite3_column_text(stmt,0)` returns a `const unsigned char*` to the first character of the first column of the current row of a result set of a prepared statement.
