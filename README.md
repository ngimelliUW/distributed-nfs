# Distributed File Server

## Overview

This project implements a stand-alone, UDP-based distributed file server, `MFS_Server`, alongside a client library, `libmfs.so`. The file server handles file operations on a virtual file system image stored on disk, while the client library provides APIs for applications to interact with the server.

## Components

- **MFS_Server:** A UDP server that listens for incoming file operation requests and processes them accordingly. It utilizes a fixed-sized on-disk file, known as the *file system image*, to persistently store data.
  
- **libmfs.so:** A shared library providing an API interface for applications to perform file operations on the server. This library handles network communication and provides fault tolerance through operation retries with timeouts.

## File System Structure

The on-disk file system adopts a simple Unix-like structure as detailed in the provided `ufs.h` header file:
- **Super Block:** The first 4KB block of the file system image.
- **Bitmaps:** Blocks that track the allocation status of inodes and data blocks.
- **Inode Table:** Stores metadata for each file and directory.
- **Data Region:** Actual data blocks that store file content or directory entries.

## API

The client library supports the following operations:
- `MFS_Init(char *hostname, int port)`: Initializes a connection to the file server.
- `MFS_Lookup(int pinum, char *name)`: Retrieves the inode number for a file/directory within a directory.
- `MFS_Stat(int inum, MFS_Stat_t *m)`: Fetches metadata for a specific inode.
- `MFS_Write(int inum, char *buffer, int offset, int nbytes)`: Writes data to a file.
- `MFS_Read(int inum, char *buffer, int offset, int nbytes)`: Reads data from a file or directory.
- `MFS_Creat(int pinum, int type, char *name)`: Creates a new file or directory.
- `MFS_Unlink(int pinum, char *name)`: Removes a file or directory.
- `MFS_Shutdown()`: Shuts down the server and ensures all data is synchronized to disk.

## Server Idempotency

To ensure reliability, the server commits all modified data to disk before confirming successful operations. This idempotency principle allows the client to safely retry operations in case of communication failures or server crashes.

## Usage

To run the server:
- `portnum`: Port number the file server will listen on.
- `file-system-image`: Path to the file system image file.

## Build Instructions

Refer to the provided Makefile for building the server and client library. Ensure that the required dependencies for UDP communication and file system manipulation are installed.

## Documentation

For a deeper understanding of the file system implementation and the client-server communication model, refer to the following resources:
- [File System Implementation](https://pages.cs.wisc.edu/~remzi/OSTEP/file-implementation.pdf)
- [Distributed Systems Introduction](https://pages.cs.wisc.edu/~remzi/OSTEP/dist-intro.pdf)
- [Distributed File System: NFS](https://pages.cs.wisc.edu/~remzi/OSTEP/dist-nfs.pdf)

## Acknowledgments

This project was inspired by concepts taught in the Operating Systems courses and guided by resources from [ostep-projects](https://github.com/remzi-arpacidusseau/ostep-projects).

## Author

Developed by Nicolas Gimelli, leveraging the foundational works provided by the OSTEP community and existing open-source projects.

