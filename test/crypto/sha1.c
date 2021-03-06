#include <crypto/sha1.h>
#include <string.h>
#include <printf.h>

void dump(uint8_t hash[20])
{
	for(int i = 0; i < 20; i++)
		tracef("%02X ", hash[i]);
	tracef("\n");
}

int printable(char* msg)
{
	char* p;

	for(p = msg; *p; p++)
		if(*p & 0x80 || *p < 0x20)
			return 0;

	return 1;
}

int test(char* msg, uint8_t hash[20])
{
	uint8_t temp[20];

	sha1(temp, msg, strlen(msg));

	int diff = memcmp(hash, temp, 20);

	if(!printable(msg))
		msg = "<non-printable>";

	if(!diff) {
		tracef("OK %s\n", msg);
	} else {
		tracef("FAIL %s\n", msg);

		dump(hash);
		dump(temp);
	}

	return diff;
}

struct test {
	char* input;
	uint8_t hash[20];
} tests[] = {
	/* Empty input */
	{ "", {
		0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
		0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09 } },
	/* RFC test */
	{ "abc", {
		0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
		0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D } },
	/* High bit test */
	{ "\xFF\xFE\xFD\xFC", {
		0x00, 0x37, 0xc9, 0xb5, 0x68, 0x3c, 0x0e, 0x8d, 0x8c, 0xa6,
		0x71, 0x4d, 0xb2, 0xf1, 0x51, 0x9b, 0x23, 0x2e, 0x10, 0xe2 } },
	/* 54 bytes */
	{ "000000000011111111112222222222333333333344444444445555", { 
		0xe7, 0xef, 0x45, 0x39, 0x5b, 0xfe, 0xa4, 0xbc, 0x0a, 0xe2,
		0xf1, 0x25, 0x7d, 0x33, 0xa7, 0x15, 0xb3, 0x9e, 0x76, 0x63 } },
	/* 55 bytes, no pad2 */
	{ "0000000000111111111122222222223333333333444444444455555", { 
		0x99, 0x76, 0xba, 0x9d, 0x64, 0x55, 0x47, 0x17, 0x8e, 0x8c,
		0x7c, 0x62, 0x55, 0x8d, 0x9d, 0x32, 0x88, 0x58, 0x48, 0xd3 } },
	/* 56 bytes, pad2 here */
	{ "0000000000111111111122222222223333333333444444444455555x", { 
		0x7c, 0x9a, 0xa3, 0x81, 0x79, 0xf8, 0x7d, 0xd7, 0xc4, 0xd0,
		0xd9, 0xa5, 0x0c, 0xfe, 0x64, 0x9d, 0xcb, 0xaa, 0x58, 0x2e } },
	/* 62 */
	{ "01234567012345670123456701234567012345670123456701234567012345", { 
		0xc0, 0x6a, 0xd7, 0x2e, 0x92, 0xc1, 0x11, 0xc5, 0xf8, 0xab,
		0xf2, 0x63, 0xb4, 0x2b, 0xbf, 0xf9, 0x13, 0x24, 0x4d, 0x4f } },
	/* 63, pad trailing byte then continue with a new block */
	{ "01234567012345670123456701234567012345670123456701234567012345z", { 
		0x79, 0x92, 0x87, 0x66, 0x91, 0x4b, 0xb9, 0x75, 0x10, 0xe3,
		0xf7, 0x42, 0x86, 0x24, 0x70, 0x4b, 0x67, 0xa6, 0x1c, 0x9d } },
	/* 64, pad1 with an empty block */
	{ "0123456701234567012345670123456701234567012345670123456701234567", {
		0xe0, 0xc0, 0x94, 0xe8, 0x67, 0xef, 0x46, 0xc3, 0x50, 0xef,
		0x54, 0xa7, 0xf5, 0x9d, 0xd6, 0x0b, 0xed, 0x92, 0xae, 0x83 } },
	{ NULL, { 0 } }
};

int main(void)
{
	struct test* tp;
	int failure = 0;

	for(tp = tests; tp->input; tp++)
		failure |= test(tp->input, tp->hash);

	return failure ? -1 : 0;
}
