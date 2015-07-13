#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ConvertUTF.h"

static int status = EXIT_SUCCESS;
static int ngenre;
static int ntrack;
static int nyear;
static char *prog;
static char *format = "%f\n\tTitle: %t\n\tArtist: %a\n\tAlbum: %A\n\t"
		"Year: %y\n\tTrack: %T\n\tGenre: %g\n";
static char *genre[] = { "Blues", "Classic Rock", "Country", "Dance", "Disco"
	, "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies"
	, "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial"
	, "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack"
	, "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion"
	, "Trance", "Classical", "Instrumental", "Acid", "House", "Game"
	, "Sound Clip", "Gospel", "Noise", "AlternRock", "Bass", "Soul", "Punk"
	, "Space", "Meditative", "Instrumental Pop", "Instrumental Rock"
	, "Ethnic", "Gothic", "Darkwave", "Techno-Industrial", "Electronic"
	, "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult"
	, "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle"
	, "Native American", "Cabaret", "New Wave", "Psychadelic", "Rave"
	, "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz"
	, "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk"
	, "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebob", "Latin"
	, "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock"
	, "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock"
	, "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humor", "Speech"
	, "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony"
	, "Booty Bass", "Primus", "Porn Groove", "Satire", "Slow Jam", "Club"
	, "Tango", "Samba", "Folklore", "Ballad", "Power Ballad"
	, "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo"
	, "A Capella", "Euro-House", "Dance HalL" };

void error(const char *arg, int r)
{
	status = EXIT_FAILURE;
	fprintf(stderr, "%s: %s: %s\n", prog, arg
		, r == -1 ? strerror(errno)
		: r == -2 ? "Failed to read requested number of bytes"
		: r == -3 ? "Compressed ID3 tag unsupported"
		: "ID3 tag size greater than available bytes");
}

int myread(int fd, void *buf, size_t s)
{
	ssize_t n;

	return (n = read(fd, buf, s)) == s ? 0
		: n >= 0 && read(fd, buf, 1) == 0 ? 1
		: n < 0 ? -1 : -2;
}

void print_genre(const char *s)
{
	const char *head = s;
	char *g;
	char *gprev;
	int in = 1;
	unsigned int n;

	errno = 0;
	while (*s != '\0') {
		if (sscanf(s, "(%ud)", &n) == 1
			&& n < sizeof(genre) / sizeof(*genre))
			g = genre[n];
		else if (strncmp(s, "(CR)", sizeof("(CR)") - 1) == 0)
			g = "Cover";
		else if (strncmp(s, "(RX)", sizeof("(RX)") - 1) == 0)
			g = "Remix";
		else {
			if (s == head) {
				printf("%s", s);
				break;
			}
			if (strncmp(s, "((", sizeof("((") - 1) == 0 && !in)
				s++;
			do
				putchar(*s);
			while (*++s != '(' && *s != '\0');
			in = 1;
			continue;
		}
		printf(in ? "" : "%s", gprev);
		printf(s != head ? "/" : "");
		while (*s++ != ')')
			;
		printf(*s == '\0' ? "%s" : "", g);
		in = 0;
		gprev = g;
	}
	if (errno != 0)
		fprintf(stderr, "%s: %s\n", prog, strerror(errno));
}

int print(const char *filepath, const char *title, const char *artist
	, const char *album, const char *year, char *track
	, const char *genre, int convert)
{
	static char s[2];
	char *pf;
	int p = 0;
	int bs = 0;
	
	for (pf = format; *pf != '\0'; pf++) {
		*s = *pf;
		if (convert && *pf == 'g' && p) {
			if (genre != NULL)
				print_genre(genre);
		} else if (printf("%s",
			*pf == 'n' && bs ? "\n"
			: *pf == 't' && bs ? "\t"
			: *pf == 'f' && p ? filepath == NULL ? "" : filepath
			: *pf == 't' && p ? title == NULL ? "" : title
			: *pf == 'a' && p ? artist == NULL ? "" : artist
			: *pf == 'A' && p ? album == NULL ? "" : album
			: *pf == 'y' && p ? year == NULL ? "" : year
			: *pf == 'T' && p ? track == NULL ? "" : track
			: *pf == 'g' && p ? genre == NULL ? "" : genre
			: *pf == '%' ? (p ? "%" : "")
			: *pf == '\\' ? (bs ? "\\" : "")
			: s) < 0) {
			fprintf(stderr, "%s: %s\n", prog, strerror(errno));
			return -1;
		}
		p = *pf == '%' && !p;
		bs = *pf == '\\' && !bs;
	}
	return 0;
}

int id3v2(const char *filepath, int fd, const unsigned char *head)
{
	int version = head[3];
	int extra;
	int r;
	int pre = version == 2 ? 6 : 10;
	unsigned char *tag;
	static char *id[][6] = { { "TT2", "TP1", "TAL", "TYE", "TRK", "TCO" }
		, { "TIT2", "TPE1", "TALB", "TYER", "TRCK", "TCON" }
		, { "TIT2", "TPE1", "TALB", "TDRC", "TRCK", "TCON" } };
	char *s[6];
	char *p[6];
	size_t size;
	size_t i;
	size_t j;
	size_t k;
	size_t frame_size;
	size_t new_frame_size;
	const UTF16 *ss;
	const UTF16 *se;
	UTF8 *ts;
	UTF8 *te;

	size = head[9] + (head[8] << 7) + (head[7] << 14) + (head[6] << 21);
	/* Compressed tags are unsupported. */
	if (version == 2 && head[5] & 0x40U)
		return -2;
	if ((tag = malloc(size)) == NULL)
		return -1;
	if ((r = myread(fd, tag, size)) != 0) {
		free(tag);
		return r == 1 ? -4 : r;
	}
	/* Reverse unsynchronization. */
	if ((version == 2 || version == 3) && head[5] & 0x80U) {
		for (i = 0, j = 0; i < size; i++)
			if (i == 0 || tag[i - 1] != 0xFFU || tag[i] != 0)
				tag[j++] = tag[i];
		size = j;
	}
	memset(s, 0, sizeof(s));
	memset(p, 0, sizeof(p));
	/* Skip the extended header and its size. */
	for (i = version == 3 && head[5] & 0x40U && size > 3
		? 4 + tag[3] + (tag[2] << 8) + (tag[1] << 16) + (tag[0] << 24)
		: version == 4 && head[5] & 0x40U && size > 3
		? 4 + tag[3] + (tag[2] << 7) + (tag[1] << 14) + (tag[0] << 21)
		: 0; i + pre < size; i += pre + frame_size) {
		frame_size = version == 2
			? tag[i + 5] + (tag[i + 4] << 8) + (tag[i + 3] << 16)
			: version == 3
			? tag[i + 7] + (tag[i + 6] << 8) + (tag[i + 5] << 16)
			+ (tag[i + 4] << 24)
			: tag[i + 7] + (tag[i + 6] << 7) + (tag[i + 5] << 14)
			+ (tag[i + 4] << 21);
		if (frame_size == 0 || i + pre + frame_size > size)
			continue;
		for (j = 0; j < 6 && strncmp((char *) tag + i
			, id[version - 2][j]
			, sizeof(id[version - 2][j]) - 1) != 0; j++)
			;
		if (j == 6)
			continue;
		/* Duplicate text frames are forbidden. */
		if (s[j] != NULL)
			continue;
		/* Skip encrypted and compressed frames. */
		if (version == 3 && tag[i + 9] & 0xC0U)
			continue;
		if (version == 4 && tag[i + 9] & 0x0CU)
			continue;
		/* Skip group identifier byte and data length indicator. */
		extra = version == 4 ? (tag[i + 9] & 0x40U ? 1 : 0)
			+ (tag[i + 9] & 0x01U ? 4 : 0) : 0;
		/* Reverse frame unsynchronization. */
		new_frame_size = frame_size;
		if (version == 4 && tag[i + 9] & 0x02)
			for (k = i + pre, new_frame_size = 0
				; k < i + pre + frame_size; k++)
				if (k == i + pre || tag[k - 1] != 0xFFU
					|| tag[k] != 0)
					tag[i + pre + new_frame_size++]
						= tag[k];
		if ((s[j] = malloc((tag[i + pre + extra] == 1
			|| (version == 4 && tag[i + pre + extra] == 2)
			? 3 * (new_frame_size - 1) / 2 + 1
			: new_frame_size) - extra)) == NULL) {
			free(tag);
			for (j = 0; j < 6; j++)
				free(s[j]);
			return -1;
		}
		/* UTF-16 possibly with BOM or UTF-16BE without BOM */
		if (tag[i + pre + extra] == 1
			|| (version == 4 && tag[i + pre + extra] == 2)) {
			ss = (UTF16 *) (tag + i + pre + extra + 1);
			se = (UTF16 *) (tag + i + pre + new_frame_size);
			ts = (UTF8 *) s[j];
			te = ts + 3 * (new_frame_size - 1 - extra) / 2;
			if (ConvertUTF16toUTF8(&ss, se, &ts, te
				, strictConversion) != conversionOK) {
				fprintf(stderr, "%s: %s: Unicode conversion " 
					"failure\n", prog, filepath);
				status = EXIT_FAILURE;
				free(s[j]);
				s[j] = NULL;
				continue;
			}
			/* Defensive null-terminator */
			ts[0] = '\0';
			p[j] = new_frame_size - extra >= 3
				&& (unsigned char) s[j][0] == 0xEFU
				&& (unsigned char) s[j][1] == 0xBBU
				&& (unsigned char) s[j][2] == 0xBFU
				? s[j] + 3 : s[j];
		/* ISO-8859-1 or UTF-8 */
		} else {
			strncpy(s[j], (char *) tag + i + pre + extra + 1
				, new_frame_size - 1 - extra);
			s[j][frame_size - 1 - extra] = '\0';
			p[j] = s[j];
		}
	}
	if (p[3] != NULL && !nyear)
		strtok(p[3], "-");
	if (p[4] != NULL && !ntrack)
		strtok(p[4], "/");
	print(filepath, p[0], p[1], p[2], p[3], p[4], p[5], !ngenre);
	free(tag);
	for (j = 0; j < 6; j++)
		free(s[j]);
	return 0;
}

int doarg(const char *pathname, int fd)
{
	int r;
	unsigned char genre_off = 0; 
	unsigned char head[10];
	char id3[128];
	static char title[31];
	static char artist[31];
	static char album[31];
	static char year[5];
	static char comment[31];
	static char track[4];
	char *pid3 = id3;

	if ((r = myread(fd, head, sizeof(head))) != 0)
		return r;
	if (strncmp((char *) head, "ID3", sizeof("ID3") - 1) == 0
		&& head[3] >= 2 && head[3] <= 4)
		return id3v2(pathname, fd, head);
	if (lseek(fd, -sizeof(id3), SEEK_END) < 0)
		return errno == EINVAL ? 1 : -1;
	if ((r = myread(fd, id3, sizeof(id3))) != 0)
		return r;
	if (strncmp(pid3, "TAG", sizeof("TAG") - 1) != 0)
		return 1;
	strncpy(title, pid3 += sizeof("TAG") - 1, 30);
	strncpy(artist, pid3 += 30, 30);
	strncpy(album, pid3 += 30, 30);
	strncpy(year, pid3 += 30, 4);
	strncpy(comment, pid3 += 4, 30);
	genre_off = (unsigned char) *(pid3 += 30);
	track[0] = '\0';
	if (comment[28] == '\0' && comment[29] != '\0'
		&& sprintf(track, "%d", (unsigned char) comment[29]) < 0)
		return -1;
	print(pathname, title, artist, album, year, track
		, genre_off > 125 ? "" : genre[genre_off], 0);
	return 0;
}

int main(int argc, char *argv[])
{
	int c;
	int fd;
	int r;

	prog = argv[0];
	while ((c = getopt(argc, argv, "gtyf:")) != -1)
		switch (c) {
		case 'g':
			ngenre = 1;
			break;
		case 't':
			ntrack = 1;
			break;
		case 'y':
			nyear = 1;
			break;
		case 'f':
			format = optarg;
			break;
		default:
			exit(EXIT_FAILURE);
		}
	for (argv += optind; *argv != NULL; argv++) {
		if ((fd = open(*argv, O_RDONLY, 0)) < 0) {
			error(*argv, -1);
			continue;
		}
		if ((r = doarg(*argv, fd)) < 0)
			error(*argv, r);
		else if (r == 1)
			fprintf(stderr, "%s: %s: No ID3 tag found\n", prog
				, *argv);
		if (close(fd) < 0)
			error(*argv, -1);
	}
	return status;
}
