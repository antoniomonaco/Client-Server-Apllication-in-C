# File Transfer Application (myFTserver & myFTclient)

## Overview

This project implements a client-server application that allows for transferring files between a client and a server. The application supports the following operations:
- Upload (write) a file from the client to the server.
- Download (read) a file from the server to the client.
- List files available on the server in a specified directory.

The project consists of two main programs:
- **myFTserver**: The server-side application that manages file storage and handles requests from clients.
- **myFTclient**: The client-side application that makes requests to the server for file transfers or directory listings.

## Features

- **Concurrent Connections**: The server can handle multiple client connections simultaneously.
- **Concurrent File Access**: The server manages concurrent write access to the same file and concurrent creation of directories.
- **Error Handling**: Proper handling of common issues like missing files, invalid command-line arguments, disk space limits, connection interruptions, etc.

## Usage

### Server (`myFTserver`)

The server listens on a specified IP address and port, managing a directory where files are stored.

#### Command:
```
myFTserver -a <server_address> -p <server_port> -d <ft_root_directory>
```

- **server_address**: IP address of the server.
- **server_port**: Port number on which the server will listen.
- **ft_root_directory**: Root directory on the server for storing and retrieving files. This directory is created if it doesn't exist.

#### Example:
```
myFTserver -a 127.0.0.1 -p 8080 -d /home/user/fileserver
```

### Client (`myFTclient`)

The client can send file transfer requests to the server or list files in a directory on the server.

#### Write (Upload) a File:
```
myFTclient -w -a <server_address> -p <port> -f <local_path/filename_local> [-o <remote_path/filename_remote>]
```

- **local_path/filename_local**: Local file path to be uploaded.
- **remote_path/filename_remote** (optional): Remote file name and directory to store the file on the server. If not provided, the local file path is used.

#### Example:
```
myFTclient -w -a 127.0.0.1 -p 8080 -f ./myfile.txt -o /remote/dir/serverfile.txt
```

#### Read (Download) a File:
```
myFTclient -r -a <server_address> -p <port> -f <remote_path/filename_remote> [-o <local_path/filename_local>]
```

- **remote_path/filename_remote**: Remote file path on the server.
- **local_path/filename_local** (optional): Local file path to store the downloaded file. If not provided, the remote file path is used.

#### Example:
```
myFTclient -r -a 127.0.0.1 -p 8080 -f /remote/dir/serverfile.txt -o ./myfile.txt
```

#### List Files:
```
myFTclient -l -a <server_address> -p <port> -f <remote_path/>
```

- **remote_path/**: Directory on the server to list files from.

#### Example:
```
myFTclient -l -a 127.0.0.1 -p 8080 -f /remote/dir/
```

### Error Handling

The following errors are handled by both the server and client:
- Invalid command-line arguments.
- File not found (for reading).
- Insufficient disk space (on both client and server).
- Connection failures.
- Concurrent write access to the same file.

## Installation and Compilation

1. Clone the repository:
   ```
   git clone https://github.com/username/myFTproject.git
   cd myFTproject
   ```

2. Compile the client and server programs:
   ```
   gcc -o myFTserver server.c -lpthread
   gcc -o myFTclient client.c
   ```

3. Run the server on the desired machine:
   ```
   ./myFTserver -a 127.0.0.1 -p 8080 -d /path/to/server_directory
   ```

4. Run the client from another machine or the same machine:
   ```
   ./myFTclient -w -a 127.0.0.1 -p 8080 -f /path/to/local_file -o /remote/directory/file_on_server
   ```
