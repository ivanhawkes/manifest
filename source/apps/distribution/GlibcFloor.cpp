// Each call to a function that glibc gave a new version after 2.39, the distribution's floor
// (docs/stage-5-recovery-plan.md, "The runtime distribution contract"), comes here by --wrap
// (CMakeLists.txt), and is made to the version 2.39 has. scripts/check-distribution.sh fails on any
// symbol newer than the floor, which a toolchain's glibc brings in, to be bound here too.

// The wrapper of __inet_pton_chk calls inet_pton itself, which fortified would be __inet_pton_chk.
#undef _FORTIFY_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstddef>
#include <cstdlib>

// glibc 2.43 versioned remainder and sqrtf again; 2.2.5 is the first x86-64 version of each.
__asm__(".symver GlibcRemainder, remainder@GLIBC_2.2.5");
__asm__(".symver GlibcSqrtf, sqrtf@GLIBC_2.2.5");
extern "C" double GlibcRemainder(double x, double y);
extern "C" float GlibcSqrtf(float x);

// The names --wrap gives the linker, which are reserved identifiers.
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" double __wrap_remainder(double x, double y)
{
	return GlibcRemainder(x, y);
}


extern "C" float __wrap_sqrtf(float x)
{
	return GlibcSqrtf(x);
}


// glibc 2.42's check that the address fits the destination, which _FORTIFY_SOURCE calls in place of
// inet_pton; a destination too small for the family aborts, as glibc's check does.
extern "C" int __wrap___inet_pton_chk(
    int family, const char *source, void *destination, std::size_t size)
{
	if (size < (family == AF_INET6 ? sizeof(in6_addr) : sizeof(in_addr)))
		std::abort();
	return inet_pton(family, source, destination);
}
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
