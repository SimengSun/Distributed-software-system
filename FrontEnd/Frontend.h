//
// Created by CHENCHEN HU on 5/4/18.
//
#ifndef FINAL_PROJECT_FRONTEND_H
#define FINAL_PROJECT_FRONTEND_H

#include "Frontend.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <string.h>
#include <iostream>
#include <signal.h>
#include <pthread.h>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <ctime>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sstream>
#include <map>
#include "penncloud.pb.h"
using namespace std;
using namespace penncloud;

#define MAX_LEN 1048576
#define CMD_LEN 10
#define TIMEOUT 5

//need to init
extern struct sockaddr_in backendmaster_addr;
extern map<int,sockaddr_in> BACKEND_SERVERS;

extern map<string,sockaddr_in> backend_server_cached_map;


//common services
int handle_timeout(int sock, sockaddr_in address);

//mail services
EmailList* load_inbox_list(string name);
Email* get_email(string username, EmailHead emailhead);
int delete_mail(string username, Email mail, EmailList inbox);
Email* forward_mail(string username, Email oldmail);
Email* reply_mail(string username, Email oldmail);
Email* write_mail(string username,vector<string> receivers,string title,string timestamp,string content);
int send_mail(string username, Email email);
int send_mail_to_one(string receiver, Email email);


// util functions
vector<string> split(const string &s, char delim);
int read_line(int fd, char* buff);
int do_read(int fd, char *buf, int len);
void print_maillist(EmailList* list);
void print_mail(Email* mail);
string handle_get_metadata(int mysock, string username, string folder_path);
int handle_delete(int mysock, string rkey, string colkey);
int handle_put(int mysock, string rkey, string ckey, string put_val, int bigtable_type);
int handle_cput_metadata(int mysock, string username, string folderpath, int oldsize, unsigned char* oldval, int newsize, unsigned char * newval);
string get_user_password(string username);
bool replace(std::string& str, const std::string& from, const std::string& to);

// account related service
int login(string username, string password);
int store_sessionID(string username, string sessionID);
int reset_password(string username, string oldpassword, string newpassword);
int signup(string username, string password);

// web storage service
vector<string> get_filelist(string username, string folderpath);
int upload_file(string username, string file_name,  string target_folder, string file_content);
int download_file(string username, string folder_path, string file_name, string& file_content);
int rename_file(string username, string folder_path, string old_filename, string new_filename);
int move_file(string username, string old_folderpath, string new_folderpath, string filename);
int delete_file(string username, string folder_path, string file_name);
int create_folder(string username, string target_folder, string folder_name);
int delete_folder(string username, string target_folder, string folder_name);
int rename_folder(string username, string folder_path, string old_foldername, string new_foldername);
int move_folder(string username, string old_foldername, string new_foldername, string folder_name);

#endif //FINAL_PROJECT_FRONTEND_H
