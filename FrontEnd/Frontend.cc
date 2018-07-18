#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <arpa/inet.h>
#include <climits>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <iostream>
#include <sys/time.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include <set>
#include <vector>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <queue>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <ctime>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <thread>
#include <streambuf>
#include <random>

#include "Frontend.h"
using namespace std;

//count the number of clients on this server
int client_num = 0;

bool debug = false;
const static int MAXBUFF = 1 << 21;
const static int BUF_SIZE = 10000;
const static int MAX_CLIENT_NUM = 100;

struct sockaddr_in backendmaster_addr;
map<int,sockaddr_in> BACKEND_SERVERS;
map<string,sockaddr_in> backend_server_cached_map;



// contants
const static string CONNECTION = "Connection: ";
const static string CONTENT_TYPE = "Content-Type: ";
const static string CONTENT_LENGTH = "Content-Length: ";
const static string COOKIE = "Cookie: ";
const static string SET_COOKIE = "Set-Cookie: ";
const static string OK_STATUS = "200 OK";


struct login_struct {
    string username;
    string password;
    // should also include master node info
};




class ChildrenThread{
public:
    int comm_fd;
    int port;
    int status; // 0--> some error, 1-->ok

    int contentLen;
    char buffer[MAXBUFF];
    char command[MAXBUFF];

    pthread_t thread;

    string method;
    string path;
    string http_version;
    string cookie_rcvd;
    string sessionID;

    string content_type;
    string boundary;

    //----------------
    string username;
    string password;

    //response
    string response_status;
    string response_server;
    string response_date;
    string response_mes_type;
    string response_mes_len;
    string response_content;
    string response_last_modified;

    bool sockOpen;
    bool logged_in;
    bool logged_out;

    //email services
    EmailList* inboxlist;
    Email* cur_mail;


    //store the commands
    vector<string> command_vec;

    //constructor
    ChildrenThread(){
        comm_fd = 0;
        port = 0;
        sockOpen = true;
        status = 1;
        inboxlist = NULL;
        cur_mail = NULL;

    }


    //main loop of the thread
    void threadMain(){
        if(debug) fprintf(stderr, "[%d] New connection\n", comm_fd);

        while(this->sockOpen){
            memset(command, 0 , strlen(command));
            logged_in = false;

            // read the http request------------------
            echo_read(comm_fd, command, 0);
            string cmd(command);

            command_vec.clear();

            //parse the http request-------------------
            parseRequest(cmd, this->command_vec);
            parseCommand();

            cout << "command_vec  " << command_vec.size() << endl;
            //cout << "command_vec addr " << &this->command_vec[0] << endl;
            cout << "command_vec!!!!" << endl;
            for (auto x: this->command_vec)
                cout << "!!!!!!" << x << endl;


            //clear the resources------------------
            response_status.clear();
            response_server.clear();
            response_date.clear();
            response_mes_type.clear();
            response_mes_len.clear();
            response_content.clear();
            response_last_modified.clear();
            logged_in = false;
            logged_out = false;

            sessionID.clear();

            //store header info
            string header;
            string myPath;

            //set headers
            this->response_server = "PennCloud Team20 Server";
            this->response_date = get_date();
            this->response_last_modified = this->response_date;


            if (debug){
                fprintf(stderr, "In respond: [%d] method: %s\nIn respond: [%d] path:\
               %s\nIn respond: [%d] http_version: %s\n", comm_fd, this->method.c_str(),\
               comm_fd, this->path.c_str(), comm_fd, this->http_version.c_str());
            }


            //GET REQUEST
            if (method.compare("GET") == 0){
                string myPath = "." + this->path;
                cout << " in GET method, path is " << myPath << endl;

                //if filepath is ./   ---> go login page
                if (myPath.compare("./") == 0){
                    myPath = "./login.html";
                    if (debug) fprintf(stderr, "[%d] Path: ./login.html\n", comm_fd);

                    //check user's session ID, if already logged in
                    //go to the menu page

//	        string ur_name = get_username(cookie_rcvd);
//		if (ur_name.compare("-ERR") == 0){
//		    myPath = "./login.html";
//		}
//		else{ // go to the menu page
//		    myPath = "./menu.html";
//	            logged_in = true;
//		}

                }


                //HTML file exists, just read the html file and return
		      if (path_valid(myPath)) {
		        if (debug) fprintf(stderr, "[%d] Path: %s is valid\n", comm_fd, myPath.c_str());
		        
		        if (myPath.compare("./download.html") == 0){
		            cout << "==========get_download_view================" << endl;
		            
		            string folderpath = "./rootdir/";
		            vector<string> file_vec = get_filelist(username, folderpath);
		            string files = "";
		            
		            cout << file_vec.size() << endl;
		            for (int i = 0; i < file_vec.size(); i++){
		                files += file_vec[i] + "<br>";
		            }
		            
		            string ppath = "./download.html";
		            string file_content = get_content(ppath);
		            myReplace(file_content, "***Files***", files);
		            
		            int fileLen = file_content.length();
		            this->response_status = OK_STATUS;
		            this->response_mes_type = get_content_type(myPath);
		            this->response_mes_len = to_string(fileLen);
		            this->response_content = file_content;
		        }
		        else{
		            string file_content = get_content(myPath);
		            
		            int fileLen = file_content.length();
		            this->response_status = OK_STATUS;
		            this->response_mes_type = get_content_type(myPath);
		            this->response_mes_len = to_string(fileLen);
		            this->response_content = file_content;
		        }
		        
    			}
                else { //GET some URL, handle the requet

                    //TODO

                    if (myPath.compare("./get_email_list") == 0){
                        cur_mail = NULL;
                        this->inboxlist = load_inbox_list(username);
                        string retn = "[";
                        if (this->inboxlist!= NULL){
                            for (int i = 0; i< inboxlist->head_size();i++){
                                EmailHead head = inboxlist->head(i);
                                string ID = to_string(i);
                                string sender = head.sender();
                                string time = head.timestamp();
                                string title = head.title();
                                retn += "{ID:"+ID+","+"sender:"+sender+","+"title:"+title+","+"time:"+time+"},";
                            }
                            retn.pop_back();
                        }
                        retn +="]";

                        if (inboxlist != NULL)
                            escape_json(retn);
                        cout << "int get email list json result: " << retn << endl;
                        //TODO: Append string to json
                        //retn = "[{\"ID:\"1\", \"sender\": \"shifan\", \"title\": \"test\", \"time\": \"today\"}, {\"ID\":\"2\", \"sender\": \"wenxi\", \"title\": \"test2\", \"time\": \"tomorrow\"}]";
                        //if (inboxlist == NULL) retn = "{}";
                        this->response_content = retn;
                        this->response_status = OK_STATUS;
                        this->response_mes_type = "application/json";
                        this->response_mes_len = to_string(retn.length());
                    }

                    if (myPath.compare("./get_email_view") == 0){
                        if (cur_mail!= NULL){
                        	cout << "in get email view cur_mail is not null" << endl;
                            string content = cur_mail->data();
                            string sender = cur_mail->head().sender();
                            string title = cur_mail->head().title();
                            string time = cur_mail->head().timestamp();
                            replace( time.begin(), time.end(), ',', '-');
                            replace(time.begin(),time.end(),':','-');
                            string retn = "[{content:"+content+","+"sender:"+sender+","+"title:"+title+","+"time:"+time+"}]";
                            escape_json(retn);
                            this->response_content = retn;
                            this->response_status = OK_STATUS;
                            this->response_mes_type = "application/json";
                            this->response_mes_len = to_string(retn.length());
                        }else{
                        	cout << "in get email view cur_mail is null" << endl;
                        	this->response_status = "404 ERR";
                        	string file_content = "";
                            int fileLen = file_content.length();
                        	this->response_mes_type = "text/plain";
                        	this->response_mes_len = to_string(fileLen);
                        	this->response_content = file_content;
                        }
                        //TODO
//                        string retn = "[{\"content\":\"this is the test content\", \"sender\": \"shifan\", \"title\": \"test\", \"time\": \"today\"}]";

                    }

                    if (myPath.compare("./reply") == 0){
                      cout << "handleing reply" << endl;
                        if (cur_mail!= NULL){
                            Email* replymail = reply_mail(username,*cur_mail);
                            if (replymail!= NULL){
                                string content = replymail->data();
                                string retn = "[{\"content\":\""+content+"\"}]";
                                this->response_content = retn;
                                this->response_status = OK_STATUS;
                                this->response_mes_type = "application/json";
                                this->response_mes_len = to_string(retn.length());
                            }
                        }else{
                        	this->response_status = "404 ERR";
                        	string file_content = "";
                            int fileLen = file_content.length();
                        	this->response_mes_type = "text/plain";
                        	this->response_mes_len = to_string(fileLen);
                        	this->response_content = file_content;
                        }
              
                    }

                    if (myPath.compare("./forward") == 0){
                        if (cur_mail!= NULL){
                            Email* forwardmail = forward_mail(username,*cur_mail);
                            if (forwardmail!= NULL){
                                string content = forwardmail->data();
                                string retn = "[{\"content\":\""+content+"\"}]";
                                this->response_content = retn;
                                this->response_status = OK_STATUS;
                                this->response_mes_type = "application/json";
                                this->response_mes_len = to_string(retn.length());
                            }
                        }else{
                        	this->response_status = "404 ERR";
                        	string file_content = "";
                            int fileLen = file_content.length();
                        	this->response_mes_type = "text/plain";
                        	this->response_mes_len = to_string(fileLen);
                        	this->response_content = file_content;
                        }
                    }

                    if (myPath.compare("./delete") ==0){
                        if (inboxlist != NULL && cur_mail != NULL){
                        	cout << "inboxlist and cur_mail is not null" << endl;
                            int result = delete_mail(username,*cur_mail,*inboxlist);
                            if (result < 0){
                            	cout << "delete mail failed" << endl;
                                myPath = "./view_mail.html";
                                string file_content = get_content(myPath);
                                int fileLen = file_content.length();
                                this->response_status = OK_STATUS;
                                this->response_mes_type = get_content_type(myPath);
                                this->response_mes_len = to_string(fileLen);
                                this->response_content = file_content;
                            }else{
                            	cout << "delete mail success" << endl;
                                cur_mail = NULL;
                                myPath = "./inbox.html";
                                string file_content = get_content(myPath);
                                int fileLen = file_content.length();
                                this->response_status = OK_STATUS;
                                this->response_mes_type = get_content_type(myPath);
                                this->response_mes_len = to_string(fileLen);
                                this->response_content = file_content;
                            }
                        }else{
                        	cout << "in delete mail, inbox list and cur_mail is null" << endl;
                        	this->response_status = "404 ERR";
                        	string file_content = "";
                            int fileLen = file_content.length();
                        	this->response_mes_type = "text/plain";
                        	this->response_mes_len = to_string(fileLen);
                        	this->response_content = file_content;
                        }

                    }
                    if (myPath.compare("./write_mail")==0){
                        myPath = "./send_mail.html";
                        string file_content = get_content(myPath);
                        int fileLen = file_content.length();
                        this->response_status = OK_STATUS;
                        this->response_mes_type = get_content_type(myPath);
                        this->response_mes_len = to_string(fileLen);
                        this->response_content = file_content;
                    }

                    if (myPath.compare("./get_console_view") == 0){
                        string retn = "[{front:[{IP:1,status:alive},{IP:1,status:alive}]},{back:[{IP:3,status:alive},{IP:4,status:killed}]}]";
                        escape_json(retn);
                        this->response_content = retn;
                        this->response_status = OK_STATUS;
                        this->response_mes_type = "application/json";
                        this->response_mes_len = to_string(retn.length());
                    }
                }
            }

                //POST REQUEST
            else if (method.compare("POST") == 0){
                string myPath = "." + this->path;
                cout << "Handling POST" <<endl;
                if (debug) fprintf(stderr, "[%d] Path: %s \n", comm_fd, myPath.c_str());


                //Handle the sign up rrquest
                if (myPath.compare("./sign_up") == 0){
                    if (debug) fprintf(stderr, "[%d] Path: %s is valid\n", comm_fd, myPath.c_str());

                    if (contain_use_pass()){
                        //need to parse the username and password
                        //cout << "111111111111111111" << endl;

                        set_user_pass();

                        if (debug){
                            cerr << "in sign up username is : " << username << endl;
                            cerr << "in sign up password is : " << password << endl;
                        }

                        //if successful, return to the login page
                        if (signup(username, password) == 0) {
                            cout << "signup: sucess!!" << endl;
                            myPath = "./login.html";
                            string file_content = get_content(myPath);

                            int fileLen = file_content.length();
                            this->response_status = OK_STATUS;
                            this->response_mes_type = get_content_type(myPath);
                            this->response_mes_len = to_string(fileLen);
                            this->response_content = file_content;


                        }
                        else{//go to user already exist page
                            cout << "signup: failed" << endl;

                            myPath = "./user_already_exist.html";
                            string file_content = get_content(myPath);
                            int fileLen = file_content.length();
                            this->response_status = OK_STATUS;
                            this->response_mes_type = get_content_type(myPath);
                            this->response_mes_len = to_string(fileLen);
                            this->response_content = file_content;
                        }
                    }
                }
                    //handle the login action
                else if (myPath.compare("./login")==0){
                    if (debug) cerr << "handling login action" << endl;

                    if (contain_use_pass()){
                        //need to parse the username and password
                        //cout << "111111111111111111" << endl;

                        set_user_pass();

                        if (debug){
                            cerr << "in login action username is : " << username << endl;
                            cerr << "in login action password is : " << password << endl;
                        }
                    }

                    if (login(username, password) == 0){
                        //login success, go to menu.html page
                    	cout <<" ====================== log in success =====================" << endl;
                        logged_in = true;
                        myPath = "menu.html";
                        sessionID = get_sessionID();
                        if (store_sessionID(username, sessionID) == 0){
                            cout << "store session successfully" << endl;

                        }
                        else{
                            cout << "store session failed" << endl;
                        }

                    }
                    else{//go to login failed page
                    	cout << " ============================ log in failed =======================" << endl;
                        myPath = "./login_failed.html";
                    }


                    string file_content = get_content(myPath);
                    int fileLen = file_content.length();
                    this->response_status = OK_STATUS;
                    this->response_mes_type = get_content_type(myPath);
                    this->response_mes_len = to_string(fileLen);
                    this->response_content = file_content;

                }
                    //handle the change_password action
                else if (myPath.compare("./change_password")==0){
                    if (debug) cerr << "handling change password" << endl;

                    if (contain_use_pass()){
                        //need to parse the username and password
                        //cout << "111111111111111111" << endl;

                        set_user_pass();

                        if (debug){
                            cerr << "in change password, username is : " << username << endl;
                            cerr << "in change password, password is : " << password << endl;
                        }
                    }

                    if (reset_password(username, "old_pass", password) == 0){
                        //success, go to menu.html page
                    	if (debug) cout << " reset password success" << endl;
                        myPath = "change_password_successfully.html";
                    }
                    else{//should not be reached
                        cerr << "reset password failed for some reason" << endl;
                    }

                    string file_content = get_content(myPath);
                    int fileLen = file_content.length();
                    this->response_status = OK_STATUS;
                    this->response_mes_type = get_content_type(myPath);
                    this->response_mes_len = to_string(fileLen);
                    this->response_content = file_content;
                }
                else if(myPath.compare("./sendemail") == 0) {
                    int ind = command_vec.size() - 1;
                    string str = command_vec[ind];
                    vector<string> v = parse_email_sending(str);
                    string sendto = v[0];
                    myReplace(sendto, "%40", "@");
                    string newsendto = sendto.substr(0,sendto.find("@"));
                    string title = v[1];
                    string content = v[2];
                    //TODO: get html timestamp
                    string timestamp = get_date();
                    replace( timestamp.begin(), timestamp.end(), ',', '-');
                    replace(timestamp.begin(),timestamp.end(),':','-');
                    replace(timestamp.begin(),timestamp.end(),' ','-');
                    if (debug)
                        cerr << "send mail to " << newsendto << " title: " << title << " content: " << content << endl;

                    vector<string> receivers;
                    receivers.push_back(newsendto);
                    Email* newmail = write_mail(username,receivers,title,timestamp,content);
                    int sendresult = send_mail(username,*newmail);
                    if (sendresult < 0){
                        myPath = "./send_mail.html";
                    }else{
                        myPath = "./inbox.html";
                    }
                    string file_content = get_content(myPath);

                    int fileLen = file_content.length();
                    this->response_status = OK_STATUS;
                    this->response_mes_type = get_content_type(myPath);
                    this->response_mes_len = to_string(fileLen);
                    this->response_content = file_content;


                }

                //view email
                else if(myPath.compare("./checkpage") == 0){
                    //TODO:
                	cout << " in POST handle checkpage" << endl;
                    int ind = command_vec.size() -1;
                    string str = command_vec[ind];
                    int id = stoi(str.substr(str.find("=")+1));
                    cout << " post handle checkpage id: " << id << endl;

                    if (inboxlist != NULL){
                    	cout << "inbox not null" << endl;
                    	EmailHead head = inboxlist->head(id);
                    	Email* mail = get_email(username, head);
                    	string file_content;
                    	if (mail != NULL){
                    		cout << "get mail success " <<endl;
                    		myPath = "./view_mail.html";
                    		this->cur_mail = mail;

                    		cout << "output filecontent " << file_content;
                            this->response_status = OK_STATUS;
                    	}else{
                    		cout << "get mail failed" << endl;
                    		myPath = "./inbox.html";
                    		this->response_status = OK_STATUS;
                    	}
                    	file_content = get_content(myPath);
                    	int fileLen = file_content.length();
                    	this->response_mes_type = get_content_type(myPath);
                    	this->response_mes_len = to_string(fileLen);
                    	this->response_content = file_content;
                    }else{
                    	cout << "inbox null" << endl;
                    	this->response_status = "404 ERR";
                    	string file_content = "";
                        int fileLen = file_content.length();
                    	this->response_mes_type = "text/plain";
                    	this->response_mes_len = to_string(fileLen);
                    	this->response_content = file_content;
                    }

                }
                //int upload_file(string username, string file_name,  string target_folder, string file_content);
				  else if (myPath.compare("./upload") == 0){
				     // int byte_read = 0;
				     string result = "";
         //             char buf[10000];
				     string filename = "";
				     // while(byte_read < contentLen){
	        //                memset(buf, 0 , strlen(buf));

	        //                echo_read(comm_fd, buf, 0);
	        //                cout << "echo read buffer " << buf << endl;
	        //                string one_part(buf);

	        //                result += one_part;
	        //                byte_read += one_part.length();
				     //       memset(buf, 0 , strlen(buf));

				     // }
				     vector<string> temp_vec;
				     parseRequest(cmd, temp_vec);

				     if (debug) cout << "temp_vec size: "<< temp_vec.size() << endl;

				         //find the name of file
				         for (int i = 0; i < temp_vec.size(); i++){
				                 if (temp_vec[i].find("filename=") != string::npos){
				                 int ind = temp_vec[i].find("filename=");
				                 filename = temp_vec[i].substr(ind+9);
				          //cout << "[i]" << temp_vec[i] << endl;

				             }
				         }

				     int position = temp_vec.size() - 3;
				     if (debug){
				         cout << "filename:" << filename << endl;
				         cout << "file length:" << result.length() << endl;
				         cout << "file is :" <<  temp_vec[position]<< endl;
				     }

				     myReplace(filename, "\"", "");
				     int flg = upload_file(username, filename, "./rootdir/", temp_vec[position]);
				     if (flg == 0){
				         //upload success
				         myPath = "./operation_success.html";
				     }
				     else{
				         myPath = "./operation_failed.html";

				     }

				            string file_content = get_content(myPath);

				           int fileLen = file_content.length();
				                   this->response_status = OK_STATUS;
				     this->response_mes_type = get_content_type(myPath);
				     this->response_mes_len = to_string(fileLen);
				     this->response_content = file_content;
				  }

				                //int create_folder(string username, string target_folder, string folder_name);
				  else if (myPath.compare("./newfolder") == 0){
				       cout << "===============In newfolder===============" << endl;

				      //if success, go success page
				      string target_folder = "";
				      string folder_name = "";

				      //parse the target folder and the folder_name
				      int ind = command_vec.size() - 1;
				      string body_mss = command_vec[ind];
		              auto split = body_mss.find("&");

		              target_folder = body_mss.substr(13,split-13);
		              folder_name = body_mss.substr(split+12);

		              myReplace(target_folder, "%2F", "/");

				      if (debug){
				          cout << "target_folder: "  <<  target_folder << endl;
				          cout << "folder_name: "  <<   folder_name  << endl;
				      }



				      int flg = create_folder(username, target_folder, folder_name);

				      if (flg == 0){
				         //upload success
				         myPath = "./operation_success.html";
				     }
				     else{
				         myPath = "./operation_failed.html";

				     }

				     string file_content = get_content(myPath);

				           int fileLen = file_content.length();
				                   this->response_status = OK_STATUS;
				     this->response_mes_type = get_content_type(myPath);
				     this->response_mes_len = to_string(fileLen);
				     this->response_content = file_content;


				  }

				                //int move_file(string username, string old_folderpath, string new_folderpath, string filename);
				  else if (myPath.compare("./movefile") == 0){
				      //if success, go success page
				      cout << "=============In movefile=============" << endl;

				      int ind = command_vec.size() - 1;
				      string str = command_vec[ind];
				      vector<string> vec = parse_email_sending(str);
				      string old_folder_path = vec[1];
				      string new_folder_path = vec[2];
				      string filename = vec[0];

				      myReplace(old_folder_path, "%2F", "/");
				      myReplace(new_folder_path, "%2F", "/");

				      if (debug){
				          cout << "old_folder_path: "  << old_folder_path << endl;
				                        cout << "new_folder_path: "  << new_folder_path << endl;
				                        cout << "new_folder_path: "  << new_folder_path << endl;

				      }

				     int flg = move_file(username, old_folder_path, new_folder_path, filename);

				     if (flg == 0){
				         //upload success
				         myPath = "./operation_success.html";
				     }
				     else{
				         myPath = "./operation_failed.html";

				     }

		           string file_content = get_content(myPath);

		           int fileLen = file_content.length();
		              this->response_status = OK_STATUS;
				     this->response_mes_type = get_content_type(myPath);
				     this->response_mes_len = to_string(fileLen);
				     this->response_content = file_content;

				  }

				                //int rename_file(string username, string folder_path, string old_filename, string new_filename);
				  else if (myPath.compare("./renamefile") == 0){
				      //if success, go success page
				                    cout << "=============In renamefile=============" << endl;

				      int ind = command_vec.size() - 1;
				      string str = command_vec[ind];
				      vector<string> vec = parse_email_sending(str);
				                    string folder_path = vec[0];
				      string old_filename = vec[1];
				      string new_filename = vec[2];

				      myReplace(folder_path, "%2F", "/");
				      myReplace(old_filename, "\"", "");
				      myReplace(new_filename, "\"", "");

				      if (debug){
				          cout << "folder_path: "   << folder_path << endl;
				                        cout << "old_filename: "  << old_filename << endl;
				                        cout << "new_filename: "  << new_filename << endl;

				      }

				      int flg =  rename_file(username, folder_path, old_filename, new_filename);

				      if (flg == 0){
				         //upload success
				         myPath = "./operation_success.html";
				     }
				     else{
				         myPath = "./operation_failed.html";

				     }

				                   string file_content = get_content(myPath);

				           int fileLen = file_content.length();
				                   this->response_status = OK_STATUS;
				     this->response_mes_type = get_content_type(myPath);
				     this->response_mes_len = to_string(fileLen);
				     this->response_content = file_content;

				  }

				                //int delete_file(string username, string folder_path, string file_name);
				  else if (myPath.compare("./deletefile") == 0){
				      //if success, go success page

				      cout << "===============In delete===============" << endl;

				      //if success, go success page
				      string folder_path = "";
				      string file_name = "";

				      //parse the target folder and the folder_name
				      int ind = command_vec.size() - 1;
				               string body_mss = command_vec[ind];
				             auto split = body_mss.find("&");

				             folder_path = body_mss.substr(11,split-11);
				             file_name = body_mss.substr(split+10);

				      if (debug){
				          cout << "folder_path: "  <<  folder_path << endl;
				          cout << "file_name: "    <<   file_name  << endl;
				      }

				      myReplace(folder_path, "%2F", "/");

				      int flg = delete_file(username, folder_path, file_name);

				      if (flg == 0){
				         //upload success
				         myPath = "./operation_success.html";
				     }
				     else{
				         myPath = "./operation_failed.html";

				     }

				     string file_content = get_content(myPath);

				           int fileLen = file_content.length();
				                   this->response_status = OK_STATUS;
				     this->response_mes_type = get_content_type(myPath);
				     this->response_mes_len = to_string(fileLen);
				     this->response_content = file_content;



				  }
				                //int delete_folder(string username, string target_folder, string folder_name);
				  else if (myPath.compare("./deletefolder") == 0){
				      //if success, go success page

				             cout << "===============In delete Folder===============" << endl;

				      //if success, go success page
				      string target_folder = "";
				      string folder_name = "";

				      //parse the target folder and the folder_name
				      int ind = command_vec.size() - 1;
				               string body_mss = command_vec[ind];
				             auto split = body_mss.find("&");

				             target_folder = body_mss.substr(13,split-13);
				             folder_name = body_mss.substr(split+12);

				      if (debug){
				          cout << "target_folder: "  <<  target_folder << endl;
				          cout << "folder_name: "    <<   folder_name  << endl;
				      }

				      myReplace(target_folder, "%2F", "/");

				      int flg = delete_folder(username, target_folder, folder_name);

				      if (flg == 0){
				         //upload success
				         myPath = "./operation_success.html";
				     }
				     else{
				         myPath = "./operation_failed.html";

				     }

				     string file_content = get_content(myPath);

				           int fileLen = file_content.length();
				                   this->response_status = OK_STATUS;
				     this->response_mes_type = get_content_type(myPath);
				     this->response_mes_len = to_string(fileLen);
				     this->response_content = file_content;
				  }

				                //int move_folder(string username, string old_foldername, string new_foldername, string folder_name);
				  else if (myPath.compare("./movefolder") == 0){
				      //if success, go success page
				                    cout << "=============In Delete folder=============" << endl;

				      int ind = command_vec.size() - 1;
				      string str = command_vec[ind];
				      vector<string> vec = parse_email_sending(str);
				                    string old_foldername = vec[0];
				      string new_foldername = vec[1];
				      string folder_name = vec[2];

				      myReplace(new_foldername, "%2F", "/");
				      myReplace(old_foldername, "%2F", "/");

				      if (debug){
				          cout << "old_foldername: "   << old_foldername << endl;
				                        cout << "new_foldername: "  << new_foldername << endl;
				                        cout << "folder_name: "  << folder_name << endl;

				      }

				      int flg =  move_folder(username, old_foldername, new_foldername, folder_name);

				      if (flg == 0){
				         //upload success
				         myPath = "./operation_success.html";
				     }
				     else{
				         myPath = "./operation_failed.html";

				     }

				                   string file_content = get_content(myPath);

				           int fileLen = file_content.length();
				                   this->response_status = OK_STATUS;
				     this->response_mes_type = get_content_type(myPath);
				     this->response_mes_len = to_string(fileLen);
				     this->response_content = file_content;

				  }
				                //int download_file(string username, string folder_path, string file_name, string& file_content);
				  else if (myPath.compare("./download") == 0){
				   //if success, go success page

				             cout << "===============In Download File===============" << endl;

				      //if success, go success page
				      string folder_path = "";
				      string file_name = "";

				      //parse the target folder and the folder_name
				      int ind = command_vec.size() - 1;
				               string body_mss = command_vec[ind];
				             auto split = body_mss.find("&");

				             folder_path = body_mss.substr(11,split-11);
				             file_name = body_mss.substr(split+10);

				      if (debug){
				          cout << "folder_path: "  <<  folder_path << endl;
				          cout << "file_name: "    <<   file_name  << endl;
				      }


				      string file_content = "";
				      myReplace(folder_path, "%2F", "/");
				      myReplace(file_name, "\"", "");

				      int flg = download_file(username, folder_path, file_name, file_content);

				      if (flg == 0){
				         //upload success
				         myPath = "./operation_success.html";
				     }
				     else{
				         myPath = "./operation_failed.html";

				     }
				     cout << "downloaded file content: " << file_content << endl;

				     file_content = get_content(myPath);

				           int fileLen = file_content.length();
				                   this->response_status = OK_STATUS;
				     this->response_mes_type = get_content_type(myPath);
				     this->response_mes_len = to_string(fileLen);
				     this->response_content = file_content;

				  }
            }else{
            	//TODO: HEAD ?
            	this->response_status = "200 OK";
            	string file_content = "";
				int fileLen = file_content.length();
				this->response_mes_type = "text/plain";
				this->response_mes_len = to_string(fileLen);
				this->response_content = file_content;
            }


            //Construct the header
            header += "HTTP/1.1 " + this->response_status + "\r\n";
            header += "Server: " + this->response_server + "\r\n";
            header += "Date: " + this->response_date + "\r\n";
            header += "Content-type: " + this->response_mes_type + "\r\n";
            header += "Content-Length: " + this->response_mes_len + "\r\n";
            header += "Last-Modified: " + this->response_last_modified + "\r\n";

            if (logged_in) {
                header += "Set-Cookie: " + sessionID + "\r\n";
                logged_in = false;
            }

            header += "Connection: keep-alive\r\n";;
            header += "\r\n";

            string totalResponse;
            totalResponse += header;
            totalResponse += response_content;

//            if (debug){
//                fprintf(stderr, "[%d] totalResponse is:\n %s\n", comm_fd, totalResponse.c_str());
//            }


            //convert into c_str
            char result[totalResponse.length()+1];
            strcpy(result, totalResponse.c_str());

            cerr << "writeeeeeeeeeeeeeeeeee" << endl;
            cout << "write result =============" << result << endl;
            //write to the client
            do_write(comm_fd, result, strlen(result));
        }


    }


    //shut down this thread i.e. reset all the variables. close fd
    //TODO
    void shutDown(){
        if (comm_fd != 0) close(comm_fd);
        comm_fd = 0;
        contentLen = 0;
        command_vec.clear();
        method.clear();
        path.clear();
        http_version.clear();

        username.clear();
        password.clear();

        response_status.clear();
        response_server.clear();
        response_date.clear();
        response_mes_type.clear();
        response_mes_len.clear();
        response_content.clear();
        response_last_modified.clear();
    }


private:
    //get a random number as the session ID
    string get_sessionID(){
        std::mt19937 rng;
        rng.seed(std::random_device()());
        std::uniform_int_distribution<std::mt19937::result_type> dist6(1,1000000);
        return to_string(dist6(rng));
    }

    // parse the command, get the needed value of the http request
    // Must call after the parseRequest(string& req)
    void parseCommand(){
        if(debug) printf("[%d][Command Read size: %d] %s", comm_fd, (int)strlen(command), command);
        //clear the resource variable
        status = 0;
        contentLen = 0;
        method.clear();
        path.clear();
        http_version.clear();
        content_type.clear();
        boundary.clear();

        //parse the first line of the http request
        string s = this->command_vec[0];
        //cout << "$$$$$$$$$$" << s << endl;
        vector<string> first_line_vec;
        stringstream first_line_stream(s);
        string temp;
        while (getline(first_line_stream, temp, ' ')) {
            //cout << "!!!!!" << temp << endl;
            first_line_vec.push_back(temp);
        }

        if (first_line_vec.size() != 3) {
            //set srror status
            status = 0;
            fprintf(stderr, "[%d] Wrong input: first line of HTTP request does not contain \
                three information inputs\n", comm_fd);
            return;
        }

        this->method = first_line_vec[0];
        this->path = first_line_vec[1];
        this->http_version = first_line_vec[2];

        cout << "******************PATH: " << path << endl;

        //for debugging
        if (debug) {
            fprintf(stderr, "Parse 1st line [%d] method: %s\nParse 1st line [%d] \
                path: %s\nParse 1st line [%d] http_version: %s\n", comm_fd, \
                this->method.c_str(), comm_fd, this->path.c_str(), comm_fd, \
                    this->http_version.c_str());
        }

        //read from the second line to the empty line
        for (int i = 1; i < command_vec.size(); i++){
            string t = command_vec[i];

            //read the content-length
            if (t.find("Content-Length: ") == 0){
                contentLen = stoi(t.substr(15));
                if(debug) printf("Content length: %d\n", contentLen);
            }

            //read the cookie
            if (t.find("Cookie: ") == 0) {
                this->cookie_rcvd = t.substr(8);
                if(debug) printf("Cookie Reveived: %s\n", cookie_rcvd.c_str());
            }

            //check the content type, see if it is multipart/form-data
            if (t.find("Content-Type:") == 0 ){
                if (t.find("multipart") != string::npos){

                }
                else{

                }
            }

            //if POST method, the message content is saved in the last element of command_vec

        }
    }


    //parse each line of the request into the command_vec
    void parseRequest(const string& req, vector<string>& command_vec){

        int start = 0;
        for (int i = 0; i < req.length()-1;i++){
            if (req.at(i) == '\r' && req.at(i+1) == '\n'){
                int num_chars = i - start;
                command_vec.push_back(req.substr(start, num_chars));
                start = i + 2;
            }
        }

        int num_chars = req.length() - start;
        command_vec.push_back(req.substr(start, num_chars));

        cout << "in parse request command_vec!!!!" << endl;
        for (auto x: command_vec){
            cout << "!!!!!!" << x << endl;
        }

    }

    //parse
    //{sender, receiver, content}
    vector<string> parse_email_sending(string& str){
        vector<string> result;
        int start = 0;
        int end = start;
        for (int i = 0; i < str.length();i++){
            if (str.at(i) == '='){
                if (str.at(i+1) != '&'){
                    start = i + 1;
                    end = start;
                }
            }
            else if (str.at(i) == '&'){
                end = i;
                string s  = str.substr(start, end-start);
                result.push_back(s);
            }
        }
        string t = str.substr(start);
        result.push_back(t);
        return result;
    }

    //get the folder name
    string get_foldername(string& s){
        int pos = s.find("=");
        return s.substr(pos+1);

    }

    //parse and set the username of password
    bool set_user_pass(){
        int ind = command_vec.size() - 1;
        string body_mss = command_vec[ind];
        auto split = body_mss.find("&");

        username = body_mss.substr(9,split-9);
        password = body_mss.substr(split+10);
    }


    //check if the request contains username and password information
    bool contain_use_pass(){
        int end = command_vec.size() - 1;
        string last = command_vec[end];
        if (debug)
            cerr << "last :" << last << endl;
        if (last.find("username") != string::npos && last.find("password") != string::npos){
            return true;
        }
        else
            return false;
    }

    //Escape Helper
    void myReplace(std::string& str, const std::string& oldStr, const std::string& newStr){
        std::string::size_type pos = 0u;
        while((pos = str.find(oldStr, pos)) != std::string::npos){
            cout << pos << endl;
            str.replace(pos, oldStr.length(), newStr);
            pos += newStr.length();
        }
    }

    //escape to the json format
    void escape_json(string& s){
        myReplace(s, ",","\",\"");
        myReplace(s, ":","\":\"");
        myReplace(s, "{","{\"");
        myReplace(s, "}","\"}");
        myReplace(s, "}\",\"{","},{");
        myReplace(s, "\"[", "[");
        myReplace(s, "]\"", "]");
    }



    //same as HW2
    bool do_write(int fd, char* buf, int len){
        int sent = 0 ;
        while(sent < len){
            int n = write(fd, &buf[sent], len - sent);
            if (n < 0) return false;
            sent += n;
        }
        return true;
    }

    //max size of buffer is 10000
    bool do_read(int fd, char* buf){
        int rcvd = 0;
        int max_line = 1;

        while(rcvd < 10000){
            int n = read(fd, &buf[rcvd], max_line);
            if (n < 0) return false;
            if(buf[rcvd-1] == '\r' && buf[rcvd] == '\n'){
                buf[rcvd + n] = '\0';
                return true;
            }
            rcvd += n;
        }
        return false;
    }

    //read the http request from the browser
    //actually the similar to do_read
    bool echo_read(int fd, char *buf, int buff_ptr){
        int len = 10000;
        while(true){
            int n = read(fd, &buf[buff_ptr], len-buff_ptr);
            if(n<0) return false;
            return false;
        }
        return true;
    }

    //return the content type given the path of the file
    string get_content_type(string path){
        string result;

        if (path.find(".jpeg") != string::npos)
            result = "image/jpeg";
        else if (path.find(".js") != string::npos)
            result = "application/javascript";
        else if (path.find(".png") != string::npos)
            result = "image/png";
        else if (path.find(".css") != string::npos)
            result = "text/css";
        else if (path.find("/ico") != string::npos)
            result = "text/html";
        else if (path == "./" || path.find(".html") != string::npos)
            result = "text/html";
        else
            result = "text/plain";
        return result;
    }

    //get the content of the file
    string get_content(string& file_path){
        string content;

        ifstream in(file_path);
        in.seekg(0, std::ios::end);
        content.reserve(in.tellg());
        in.seekg(0, std::ios::beg);
        content.assign((istreambuf_iterator<char>(in)),istreambuf_iterator<char>());

/*
        if (debug) {
            fprintf(stderr, "file content is :\n%s\n", content.c_str());
        }
*/

        return content;
    }


    //get the date of current time
    //format: Mon, 23 Apr 2018 14:18:06 UTCProgram
    string get_date() {
        char buf[1000];
        time_t now = time(0);
        struct tm tm = *gmtime(&now);
        strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
        string result = string(buf);
        return result;
    }

    //check if the path is valid
    bool path_valid(string path){
        ifstream in(path);
        if (!in) return false;
        else return true;
    }
};


class threadPool{
public:
    ChildrenThread ch_threads[MAX_CLIENT_NUM];

    //constructor
    threadPool(){
        for(int i = 0; i <  MAX_CLIENT_NUM; i++)
            ch_threads[i].comm_fd = 0;
    }

    //when a thread ends, reset its file descriptor
    void resetOne(ChildrenThread* ch_thread){
        ch_thread->comm_fd = 0;
    }

    //return a thread in the pool to the user
    ChildrenThread* get_thread() {
        for(int i = 0; i < MAX_CLIENT_NUM; i++){
            if(ch_threads[i].comm_fd == 0)
                return &ch_threads[i];
        }
        return nullptr; //should not reach this point
    }

    //shut down all the threads in the pool
    void shutDownPool(){
        for(int i = 0; i < MAX_CLIENT_NUM; i++)
        {
            if(ch_threads[i].comm_fd != 0 && debug)
                printf("thread index: %d, its connfd is %d",i, ch_threads[i].comm_fd);
            ch_threads[i].shutDown();
        }

    }

};


//data will send to load balancer
struct thread_data{
    string IP;
    int port;
    int client_num;
    int index;
};


//worker function of the thread
void* worker(void* arg){
    ChildrenThread* ch_thread = (ChildrenThread*)arg;
    ch_thread->threadMain();

}


//ping load balancer
void *threadPing(void *arg){
    struct thread_data* mydata;
    mydata = (struct thread_data*)arg;

    int serverInd = mydata->index;
    int cliet_num = mydata->client_num;
    int master_port = mydata->port;
    string IP = mydata->IP;

    int master_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (master_fd < 0) {
        fprintf(stderr, "Can not open socket for master\n");
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(master_port);
    inet_pton(AF_INET, IP.c_str(), &(servaddr.sin_addr));

    if (connect(master_fd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0){
        fprintf(stderr, "Can not connect to master\n");
    }

    bool pinging = true;
    while (pinging){
        try {

            string msgStr = "/ping," + to_string(serverInd) +"," + to_string(client_num) + "\r\n";
            write(master_fd, (char* )msgStr.c_str(), strlen((char* )msgStr.c_str()));
        } catch (...) {

        }
        sleep(10);
    }
}

//construct a threading pool
threadPool threadPool;


//master front node IP and port
string masterIP;
int master_port;



//divide the string divided by the ":"
pair<string, string> divide_str(string str, char c, bool flag){
    string s1 = "";
    string s2 = "";

    auto ind = str.find(c);
    if (ind == string::npos){
        if (flag == true){
            cerr << "the format of part1 char part2 is wrong" << endl;
            exit(1);
        }
    } // I already assume the format in the configuration file is correct
    else{// ind = the index of c
        s2 = str.substr(ind+1);
        s1 = str.substr(0,ind);

    }
    return pair<string, string>(s1,s2);
}


//backend server class
class Server {
public:
    sockaddr_in addr;
    in_addr address;
    int portNum;
    Server(sockaddr_in addr):addr(addr), address(addr.sin_addr), portNum(addr.sin_port){};
};



//global
int connectedClientsNum = 0;

//(IP, port)
vector<Server> server_vec;
int main (int argc, char* argv[]){
    int portNum = 0;
    int argPortNum = 0;

    string myIP = "";
    int myport;

    if (argc < 2) {
        fprintf(stderr, "*** Author: CIS505 Team:20\n");
        exit(1);
    }

    int N;
    int c;

    //get the input argument of the server
    while ((c = getopt(argc, argv, "p:v")) != -1){
        switch (c){
            case 'p':
                argPortNum = atoi(optarg);
                if (argPortNum >= 0 && argPortNum <= 65535)
                    portNum = argPortNum;
                else{
                    fprintf(stderr,"Invalid port number\n");
                    exit(1);
                }
                break;
            case 'v':
                debug = true;
                break;
            case '?':
                fprintf(stderr, "The flag is not valid. pleas use  -p or -v \n");
                exit(2);
            default:
                fprintf(stderr, "*** Author:CIS505 Team 20\n");
                abort();
        }
    }
    //get the index of the server, assume it is the lst argument
//    string config_path_b = argv[optind];
//      int serverIndex = atoi(argv[optind+1]);
//      ifstream in;

    // string config_path_f = "./front_config.txt";
    // ifstream in;
    // in.open(config_path_f);
    // if (!in) cerr << "no such file(front_config.txt)" << endl;

    // string line;
    // int line_ind = 0;


    // while(in >> line){
    // if (line_ind == 0){
    //         auto mypair = divide_str(line, ':', true);
    //         masterIP = mypair.first;
    //         master_port = stoi(mypair.second);
    // }
    // else if (serverIndex == line_ind){
    //         auto mypair = divide_str(line, ':', true);
    //         myIP = mypair.first;
    //         myport = stoi(mypair.second);
    // }
    // line_ind++;
    // }
    // in.close();

    // cout << masterIP << endl;
    // cout << to_string(master_port) << endl;
    // cout << myIP << endl;
    // cout << to_string(myport) << endl;

    // //set thread data
    // struct thread_data td;
    // td.port = master_port;
    // td.client_num = connectedClientsNum;
    // td.index = serverIndex;


    // //create a thread communicate to load balancer
    // pthread_t thread;
    // pthread_create(&thread, NULL, threadPing, &td);
    // pthread_detach(thread);


    //read the backend server information --> vector<Server> server_vec

    // string config_path_b = "back_config.txt";
    // in.open(config_path_b);

//    in.open(config_path_b);
//
//         string line;
//         int i = 0;
//         while(in >> line){
//             auto mypair = divide_str(line, ':', true);
//             string ip = mypair.first;
//             int port = stoi(mypair.second);
//
//             cout << "IP: " << ip << endl;
//             cout << "port: " << port << endl;
//
//         struct sockaddr_in servaddr_back;
//         bzero(&servaddr_back, sizeof(servaddr_back));
//         inet_pton(AF_INET, ip.c_str(), &servaddr_back.sin_addr);
//         servaddr_back.sin_port = htons(port);
//         servaddr_back.sin_family = AF_INET;
//         servaddr_back.sin_addr.s_addr = INADDR_ANY;
//         if (i==0){
//        	 backendmaster_addr = servaddr_back;
//         }else{
//        	 BACKEND_SERVERS[i++] = servaddr_back;
//         }
//         Server cur_serv(servaddr_back);
//         //TODO does the server index include 0?????
//         server_vec.push_back(cur_serv);
//         }
//         backendmaster_addr = BACKEND_SERVERS[1];

      int backendport = atoi(argv[optind]);
          //init backend master addr
          bzero(&backendmaster_addr, sizeof(backendmaster_addr));
          backendmaster_addr.sin_family = AF_INET;
          backendmaster_addr.sin_port = htons(backendport);
          inet_pton(AF_INET, "127.0.0.1", &(backendmaster_addr.sin_addr));

    //create a socket
    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
    }
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(portNum);
    bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    listen(listen_fd, 10);


    if(debug) {
        fprintf(stderr, "Frontend listening on port number %d\n", portNum);
    }

    while(1){
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof(clientaddr);
        int* fd = (int*)malloc(sizeof(int));
        *fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);

        //debug information
        if (*fd < 0) {
            fprintf(stderr, "Could not connect the comming fd %s:%d\n", \
                inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
            continue;
        }

        //optimize a little bit
        int val = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_KEEPALIVE | SO_REUSEADDR ,&val, sizeof(val));

        //parameter to the worker function
        //struct thread_data td;
        //td.debug = debug;
        //td.fd = fd;

        //get a thread from the threadPool, set the parameters
        ChildrenThread* clientThread = threadPool.get_thread();
        clientThread->comm_fd = *fd;


        pthread_create(&clientThread->thread, NULL, worker, clientThread);
        connectedClientsNum++;
        pthread_detach(clientThread->thread);
    }
    return 0;

}
