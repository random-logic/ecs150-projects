#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

void listDirectory(LocalFileSystem *fs, int inodeNumber, const string &path) {
    inode_t inode;
    if (fs->stat(inodeNumber, &inode) != 0) {
        cerr << "Error: Invalid inode " << inodeNumber << endl;
        return;
    }

    if (inode.type != UFS_DIRECTORY) {
        cerr << "Error: Inode " << inodeNumber << " is not a directory" << endl;
        return;
    }

    vector<dir_ent_t> entries(inode.size / sizeof(dir_ent_t));
    fs->read(inodeNumber, entries.data(), inode.size);

    sort(entries.begin(), entries.end(), [](const dir_ent_t &a, const dir_ent_t &b) {
        return strcmp(a.name, b.name) < 0;
    });

    cout << "Directory " << path << endl;
    for (const auto &entry : entries) {
        cout << entry.inum << '\t' << entry.name << endl;
    }
    cout << endl;

    for (const auto &entry : entries) {
        inode_t entryInode;
        if (fs->stat(entry.inum, &entryInode) == 0 && entryInode.type == UFS_DIRECTORY && strcmp(entry.name, ".") != 0 && strcmp(entry.name, "..") != 0) {
            listDirectory(fs, entry.inum, path + entry.name + "/");
        }
    }
}

string getFullPath(LocalFileSystem *fs, int inodeNumber) {
    if (inodeNumber == UFS_ROOT_DIRECTORY_INODE_NUMBER) {
        return "/";
    }
    
    vector<string> pathSegments;
    int current = inodeNumber;

    while (current != UFS_ROOT_DIRECTORY_INODE_NUMBER) {
        inode_t inode;
        if (fs->stat(current, &inode) != 0) {
            break;
        }
        
        vector<dir_ent_t> entries(inode.size / sizeof(dir_ent_t));
        fs->read(current, entries.data(), inode.size);
        
        for (const auto &entry : entries) {
            if (entry.inum == current) {
                pathSegments.push_back(entry.name);
                break;
            }
        }
        
        current = entries[1].inum; // Move to parent inode using ".."
    }

    string fullPath = "/";
    for (auto it = pathSegments.rbegin(); it != pathSegments.rend(); ++it) {
        fullPath += *it + "/";
    }
    
    return fullPath;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cout << argv[0] << ": diskImageFile" << endl;
        return 1;
    }

    Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
    LocalFileSystem *fs = new LocalFileSystem(disk);

    listDirectory(fs, UFS_ROOT_DIRECTORY_INODE_NUMBER, "/");
    return 0;
}