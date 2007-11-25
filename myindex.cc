#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <gmime/gmime.h>
#include <string.h>
#include <set>
#include <fstream>

#include "tokenizer.h"
#include "xapianglue.h"
#include "util.h"
using namespace std;

string msgid_strip(string aline)
{
   size_t l = aline.find_first_of('<');
   size_t r = aline.find_last_of('>');
   if (l<r)
     return aline.substr(l+1,r-l-1);
   return aline;
}

int main(int argc, char** argv)
{
  GMimeStream *stream;
  GMimeParser *parser;
  GMimeMessage *msg = 0;

  int fh;

  size_t unflushed_messages = 0;

  tokenizer_init();
  xapian_init();
  start_time = time(NULL);

  // g_mime_init(GMIME_INIT_FLAG_UTF8);
  // cout << argc << "  args" << endl;
  set<string> spamids;
  set<string> seenids;

  int argi;
  for (argi = 1; argi<argc; argi++) {
    // "/org/lists.debian.org/lists/debian-project/2007/debian-project-200709"
    string fn(argv[argi]);
    fh = open(fn.c_str(), O_RDONLY);
    string basename = fn.substr(fn.find_last_of('/')+1);
    int i = basename.find_last_of('-');
    string list = basename.substr(0,i);
    string yearmonth = basename.substr(i+1);
    int year = atoi(yearmonth.substr(0,4).c_str());
    int month = 0;
    if (yearmonth.length()>4)
      month = atoi(yearmonth.substr(4).c_str());

    cout << endl << list << " " << year << " ";
    if (month != 0)
      cout << month;
    cout << endl;

    spamids.clear();
    string spamfn = fn+".spam";
    ifstream spamf(spamfn.c_str());
    if (spamf) {
       string aline;
       while (getline(spamf, aline)) {
	  string::iterator i = aline.begin();
	  while (i != aline.end() && *i != ':') {
	     if (isalnum((unsigned char)*i) || strchr(".-+*", *i)) {
		*i = tolower(*i);
		i++;
	     } else {
		aline.erase(i);
	     }
	  }
	  if (aline.substr(0,21) == "skip-spam-message-id:") {
	    aline.erase(0,21);
	    i = aline.begin();
	    while (i != aline.end() && *i == ' ')
	      aline.erase(i);
	    spamids.insert(msgid_strip(aline));
	  }
       }
    } 
    // cout << "number spam msgids: " << spamids.size() << endl;
     seenids.clear();

    int msgnum = 0;
    
    stream = g_mime_stream_fs_new(fh);

    parser = g_mime_parser_new_with_stream(stream);
    g_mime_parser_set_scan_from(parser, TRUE);
    while (! g_mime_parser_eos(parser)) {
       msg = g_mime_parser_construct_message(parser);
       
       if (msg != 0) {
	  const char* raw_msgid = g_mime_message_get_header(msg, "Message-Id");
	  string msgid;
	  if (raw_msgid != NULL)
	    msgid = msgid_strip(raw_msgid);
	  else 
	    msgid = fake_msgid(msg);
	  if (msgid == "") {
	     fprintf(stderr, "\nNo msgid\n");
	  }
	  else if (seenids.find(msgid) != seenids.end()) {
	     //cerr << endl << "dupemsgid: " << msgid << endl;
	  }
	  else if (spamids.find(msgid) != spamids.end()) {
	     //cerr << endl << "spam: " << msgid << endl;
	     xapian_delete_document(list, year, month, msgnum);
	     seenids.insert(msgid);
	     msgnum++;
	  }
	  else {
	     //cerr << endl << "msgid: " << msgid << endl;
	     cout << "." << flush;
	     seenids.insert(msgid);
	     document * doc = parse_article(msg);
	     if (doc != NULL) {
		xapian_add_document(doc, list, year, month, msgnum);
		unflushed_messages++;
	     }
	     msgnum++;
	  }
	  g_object_unref(msg);
       } 
    }
     
     g_mime_stream_unref(stream);
     close(fh);
     if (unflushed_messages>100000) {
	xapian_flush();
	unflushed_messages = 0;
     }
  }
  if (unflushed_messages>0)
     xapian_flush();
  
  tokenizer_fini();
  printf("\nDONE\n");

}
