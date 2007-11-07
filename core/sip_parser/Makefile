
NAME = sip_parser

SRCS = main.cpp \
	log.cpp \
	sip_parser.cpp \
	parse_uri.cpp \
	parse_header.cpp \
	parse_common.cpp \
	parse_via.cpp \
	parse_from_to.cpp \
	parse_cseq.cpp 

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

CXXFLAGS += -g
LDFLAGS  += -g
CPPFLAGS += -D_DEBUG

all: 
	-@$(MAKE) deps && \
	  $(MAKE) sip_parser

clean:
	rm -f *.o *.d $(NAME)

deps: $(DEPS)


$(NAME): $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(OBJS)

%.o: %.cpp %.d
	$(CXX) -o $@ -c $< $(CPPFLAGS) $(CXXFLAGS)

%.d: %.cpp Makefile
	$(CXX) -MM -MT '$@ $(@:.d=.o)' $< $(CPPFLAGS) $(CXXFLAGS) > $@


ifeq '$(MAKECMDGOALS)' '$(NAME)'
include $(DEPS)
endif