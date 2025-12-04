CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread \
           -I/usr/include/x86_64-linux-gnu \
           -DLDAP_DEPRECATED=1
LDFLAGS = -lldap -llber

SERVER_SOURCES = twmailer-server.cpp Server.cpp ClientSession.cpp MailStore.cpp BlacklistManager.cpp LdapAuthenticator.cpp
CLIENT_SOURCES = twmailer-client.cpp

all: twmailer-server twmailer-client

TWMAILER_HEADERS = MailStore.h BlacklistManager.h LdapAuthenticator.h ClientSession.h Server.h

%.o: %.cpp $(TWMAILER_HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

twmailer-server: $(SERVER_SOURCES) $(TWMAILER_HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_SOURCES) $(LDFLAGS)

twmailer-client: $(CLIENT_SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENT_SOURCES)

clean:
	rm -f twmailer-server twmailer-client *.o
