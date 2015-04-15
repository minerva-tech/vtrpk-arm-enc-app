#define _WIN32_WINNT 0x0501

#include <assert.h>

//#include <fstream>
#include <algorithm>

// DIRTY HACK
#define IPV6_UNICAST_HOPS			16
#define IPV6_MULTICAST_IF			17
#define IPV6_MULTICAST_HOPS		18
#define IPV6_MULTICAST_LOOP		19
#define IPV6_JOIN_GROUP     		20
#define IPV6_LEAVE_GROUP			21
// END OF DIRTY HACK

#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/crc.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp> // i don't believe, ms vc still has no support of std::thread, crap.

#ifdef _WIN32
#include <dshow.h>
#include <d3d9.h>
#include <vmr9.h>
#endif

#include <boost/foreach.hpp>
