
NAME = sip_parser

SRCS = main.cpp \
	log.cpp \
	sip_parser.cpp \
	parse_uri.cpp \
	parse_request.cpp \
	parse_header.cpp \
	parse_common.cpp \
	parse_via.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

CXXFLAGS += -g
LDFLAGS  += -g
CPPFLAGS += -D_DEBUG

.PHONY: all
all: deps sip_parser

.PHONY: deps
deps: $(DEPS)

$(NAME): $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(OBJS)

%.o: %.cpp %.d
	$(CXX) -o $@ -c $< $(CPPFLAGS) $(CXXFLAGS)

%.d: %.cpp Makefile
	$(CXX) -MM -MT '$@ $(@:.d=.o)' $< $(CPPFLAGS) $(CXXFLAGS) > $@


include $(DEPS)
