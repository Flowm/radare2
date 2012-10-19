/* radare - LGPL - Copyright 2007-2012 pancake<nopcode.org> */

#include "r_types.h"
#include "r_util.h"
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#if __UNIX__
#include <sys/mman.h>
#endif

R_API const char *r_file_basename (const char *path) {
	const char *ptr = strrchr (path, '/');
	if (ptr) path = ptr + 1;
	return path;
}

R_API boolt r_file_is_directory(const char *str) {
	struct stat buf;
	if (stat (str, &buf)==-1)
		return R_FALSE;
	return ((S_IFDIR &buf.st_mode))? R_TRUE: R_FALSE;
}

R_API boolt r_file_exists(const char *str) {
	struct stat buf;
	if (stat (str, &buf)==-1)
		return R_FALSE;
	return (S_ISREG (buf.st_mode))? R_TRUE: R_FALSE;
}

R_API int r_file_size(const char *str) {
	struct stat buf;
	if (stat (str, &buf)==-1)
		return 0;
	return buf.st_size;
}

R_API char *r_file_abspath(const char *file) {
	char *ret = NULL;
	char *cwd = r_sys_getdir ();
#if __UNIX__
	if (cwd && *file != '/')
		ret = r_str_dup_printf ("%s/%s", cwd, file);
#elif __WINDOWS__
	if (cwd && !strchr (file, ':'))
		ret = r_str_dup_printf ("%s/%s", cwd, file);
#endif
	free (cwd);
// TODO: remove // and ./
	return ret? ret: strdup (file);
}

R_API char *r_file_path(const char *bin) {
	char file[1024];
	char *path_env = (char *)r_sys_getenv ("PATH");
	char *path = NULL;
	char *str, *ptr;
	if (path_env) {
		str = path = strdup (path_env);
		do {
			ptr = strchr (str, ':');
			if (ptr) {
				*ptr = '\0';
				snprintf (file, sizeof (file), "%s/%s", str, bin);
				if (r_file_exists (file)) {
					free (path);
					free (path_env);
					return strdup (file);
				}
				str = ptr + 1;
			}
		} while (ptr);
	}
	free (path_env);
	free (path);
	return strdup (bin);
}

R_API char *r_file_slurp(const char *str, int *usz) {
	size_t rsz;
	char *ret;
	FILE *fd;
	long sz;
	if (!r_file_exists (str))
		return NULL;
	fd = r_sandbox_fopen (str, "rb");
	if (fd == NULL)
		return NULL;
	fseek (fd, 0, SEEK_END);
	sz = ftell (fd);
	if (sz <0) {
		fclose (fd);
		return NULL;
	}
	fseek (fd, 0, SEEK_SET);
	ret = (char *)malloc (sz+1);
	rsz = fread (ret, 1, sz, fd);
	if (rsz != sz)
		eprintf ("r_file_slurp: fread: error\n");
	fclose (fd);
	ret[sz]='\0';
	if (usz)
		*usz = (ut32)sz;
	return ret;
}

R_API ut8 *r_file_slurp_hexpairs(const char *str, int *usz) {
	ut8 *ret;
	long sz;
	int c, bytes = 0;
	FILE *fd = r_sandbox_fopen (str, "r");
	if (fd == NULL)
		return NULL;
	fseek (fd, 0, SEEK_END);
	sz = ftell (fd);
	fseek (fd, 0, SEEK_SET);
	ret = (ut8*)malloc ((sz>>1)+1);
	if (!ret)
		return NULL;
	for (;;) {
		if (fscanf (fd, " #%*[^\n]") == 1)
			continue;
		if (fscanf (fd, "%02x", &c) == 1) {
			ret[bytes++] = c;
			continue;
		}
		if (feof (fd))
			break;
		free (ret);
		return NULL;
	}

	ret[bytes] = '\0';
	fclose (fd);
	if (usz) *usz = bytes;
	return ret;
}

R_API char *r_file_slurp_range(const char *str, ut64 off, int sz, int *osz) {
	char *ret;
	FILE *fd = r_sandbox_fopen (str, "rb");
	if (fd == NULL)
		return NULL;
	// XXX handle out of bound reads (eof)
	fseek (fd, off, SEEK_SET);
	ret = (char *)malloc (sz+1);
	if (ret != NULL) {
		if (osz)
			*osz = (int)(size_t)fread (ret, 1, sz, fd);
		else fread (ret, 1, sz, fd);
		ret[sz] = '\0';
	}
	fclose (fd);
	return ret;
}

R_API char *r_file_slurp_random_line(const char *file) {
	char *ptr = NULL, *str;
	int sz, i, lines = 0;
	struct timeval tv;

	if ((str = r_file_slurp (file, &sz))) {
		gettimeofday (&tv,NULL);
		srand (getpid()+tv.tv_usec);
		for (i=0; str[i]; i++)
			if (str[i]=='\n')
				lines++;
		if (lines>0) {
			lines = (rand()%lines);
			for (i=0; str[i] && lines; i++)
				if (str[i]=='\n')
					lines--;
			ptr = str+i;
			for (i=0; ptr[i]; i++)
				if (ptr[i]=='\n') {
					ptr[i]='\0';
					break;
				}
			ptr = strdup (ptr);
		}
		free (str);
	}
	return ptr;
}

R_API char *r_file_slurp_line(const char *file, int line, int context) {
	int i, lines = 0;
	int sz;
	char *ptr = NULL, *str = r_file_slurp (file, &sz);
	// TODO: Implement context
	if (str) {
		for (i=0; str[i]; i++)
			if (str[i]=='\n')
				lines++;
		if (line > lines) {
			free (str);
			return NULL;
		}
		lines = line;
		for (i=0; str[i]&&lines; i++)
			if (str[i]=='\n')
				lines--;
		ptr = str+i;
		for (i=0; ptr[i]; i++)
			if (ptr[i]=='\n') {
				ptr[i]='\0';
				break;
			}
		ptr = strdup (ptr);
		free (str);
	}
	return ptr;
}

R_API boolt r_file_dump(const char *file, const ut8 *buf, int len) {
	int ret;
	FILE *fd = r_sandbox_fopen(file, "wb");
	if (fd == NULL) {
		eprintf ("Cannot open '%s' for writing\n", file);
		return R_FALSE;
	}
	ret = fwrite (buf, 1, len, fd) == len;
	if (!ret)
		eprintf ("r_file_dump: fwrite: error\n");
	fclose (fd);
	return ret;
}

R_API boolt r_file_rm(const char *file) {
#if __WINDOWS__
	return (DeleteFile (file)==0)? R_TRUE: R_FALSE;
#else
	return (unlink (file)==0)? R_TRUE: R_FALSE;
#endif
}

// TODO: add rwx support?
R_API RMmap *r_file_mmap (const char *file, boolt rw) {
	RMmap *m = NULL;
#if __WINDOWS__
	int fd = r_sandbox_open (file, O_BINARY, 0644);
#else
	int fd = r_sandbox_open (file, rw? O_RDWR: O_RDONLY, 0644);
#endif
	if (fd != -1) {
		m = R_NEW (RMmap);
		if (!m) {
			close (fd);
			return NULL;
		}
		m->rw = rw;
		m->fd = fd;
		m->len = lseek (fd, (off_t)0, SEEK_END);
#if __UNIX__
		m->buf = mmap (NULL, m->len, rw?PROT_READ|PROT_WRITE:PROT_READ,
				MAP_SHARED, fd, (off_t)0);
		if (m->buf == MAP_FAILED) {
			free (m);
			m = NULL;
		}
#elif __WINDOWS__
		close (fd);
		m->fh = CreateFile (file, rw?GENERIC_WRITE:GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
		if (m->fh == NULL) {
			free (m);
			return NULL;
		}
		m->fm = CreateFileMapping (m->fh, NULL,
			rw?PAGE_READWRITE:PAGE_READONLY, 0, 0, NULL);
		if (m->fm == NULL) {
			CloseHandle (m->fh);
			free (m);
			return NULL;
		}
		if (m->fm != INVALID_HANDLE_VALUE) {
			m->buf = MapViewOfFile (m->fm, rw?FILE_MAP_READ|FILE_MAP_WRITE:FILE_MAP_READ, 0, 0, 0);
		} else {
			CloseHandle (m->fh);
			free (m);
			m = NULL;
		}
#else
		m->buf = malloc (m->len);
		if (m->buf) {
			lseek (fd, (off_t)0, SEEK_SET);
			read (fd, m->buf, m->len);
		} else {
			free (m);
			m = NULL;
		}
#endif
	}
	return m;
}

R_API void r_file_mmap_free (RMmap *m) {
	if (!m) return;
#if __UNIX__
	munmap (m->buf, m->len);
#elif __WINDOWS__
	CloseHandle (m->fm);
	CloseHandle (m->fh);
	UnmapViewOfFile (m->buf);
#endif
	close (m->fd);
	free (m);
}

R_API char *r_file_temp (const char *prefix) {
	int namesz;
	char *name;
	char *path = r_file_tmpdir ();
	namesz = strlen (prefix) + strlen (path) + 32;
	name = malloc (namesz);
	snprintf (name, namesz, "%s/%s.%"PFMT64x, path, prefix, r_sys_now ());
	free (path);
	return name;
}

R_API int r_file_mkstemp (const char *prefix, char **oname) {
	int h;
	char *path = r_file_tmpdir ();
	char name[1024];
#if __WINDOWS__
	if (GetTempFileName (path, prefix, 0, name))
		h = r_sandbox_open (name, O_RDWR|O_EXCL|O_BINARY, 0644);
	else h = -1;
#else
	snprintf (name, sizeof (name), "%s/%sXXXXXX", path, prefix);
	h = mkstemp (name)!=-1? R_TRUE: R_FALSE;
#endif
	if (oname) *oname = h? strdup (name): NULL;
	free (path);
	return h;
}

R_API char *r_file_tmpdir() {
#if __WINDOWS__
	char *path = r_sys_getenv ("TEMP");
	if (!path) path = strdup ("C:\\WINDOWS\\Temp\\");
#elif __ANDROID__
	char *path = strdup ("/data/data/org.radare.installer/radare2/tmp");
#else
	char *path = r_sys_getenv ("TMPDIR");
	if (!path) path = strdup ("/tmp");
#endif
	if (!r_file_is_directory (path)) {
		eprintf ("Cannot find temporary directory '%s'\n", path);
	}
	return path;
}

