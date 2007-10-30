
OBJS = main.o log.o sip_parser.o parse_uri.o parse_request.o parse_header.o parse_common.o

CXXFLAGS += -g
LDFLAGS  += -g
CPPFLAGS += -D_DEBUG

sip_parser: $(OBJS) Makefile
	$(CXX) -o $@ $(LDFLAGS) $(OBJS)

%.o: %.cpp Makefile
	$(CXX) -o $@ -c $< $(CPPFLAGS) $(CXXFLAGS)
