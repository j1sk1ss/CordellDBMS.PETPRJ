#include "../include/common.h"


void strrand(char* dest, size_t length, int offset) {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t charset_size = sizeof(charset) - 1;

    if (length < 2) {
        if (length == 1) dest[0] = '\0';
        return;
    }

    dest[length - 1] = '\0';
    for (size_t i = length - 2; i < length; i--) {
        dest[i] = charset[offset % charset_size];
        offset /= charset_size;
    }
}

int is_integer(const char* str) {
    while (*str) {
        if (!isdigit_s(*str)) return 0;
        str++;
    }

    return 1;
}

char* strrep(char* __restrict string, char* __restrict source, char* __restrict target) {
    char* result = NULL; // the return string
    char* ins = NULL;    // the next insert point
    char* tmp = NULL;    // varies
    int len_source = 0;  // length of source (the string to remove)
    int len_target = 0;  // length of target (the string to replace source target)
    int len_front = 0;   // distance between source and end of last source
    int count = 0;       // number of replacements

    // sanity checks and initialization
    if (!string || !source) return NULL;
    len_source = strlen_s(source);
    
    if (len_source == 0) return NULL; // empty source causes infinite loop during count
    if (!target) target = "";
    len_target = strlen_s(target);

    // count the number of replacements needed
    ins = string;
    for (count = 0; (tmp = strstr_s(ins, source)); ++count) {
        ins = tmp + len_source;
    }

    tmp = result = (char*)malloc_s(strlen_s(string) + (len_target - len_source) * count + 1);
    if (!result) return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of source in string
    //    string points to the remainder of string after "end of source"
    while (count--) {
        ins = strstr_s(string, source);
        len_front = ins - string;
        tmp = strncpy_s(tmp, string, len_front) + len_front;
        tmp = strcpy_s(tmp, target) + len_target;
        string += len_front + len_source; // move to next "end of source"
    }

    strcpy_s(tmp, string);
    return result;
}

unsigned int str2hash(const char* str) {
    unsigned int hashedValue = 0;
    for (int i = 0; str[i] != '\0'; i++) hashedValue ^= (hashedValue << MAGIC) + (hashedValue >> 2) + str[i];
    for (int i = 0; i < MAGIC; i++) hashedValue ^= (hashedValue << MAGIC) + (hashedValue >> 2) + SALT[i];
    return hashedValue;
}

char* strstr_s(char* haystack, char* needle) {
	int i = 0;
	int j = 0;

	i = 0;
	if (needle[0] == '\0')
		return ((char *)haystack);
	while (haystack[i] != '\0')
	{
		j = 0;
		while (haystack[i + j] == needle[j] && haystack[i + j] != '\0')
		{
			if (needle[j + 1] == '\0')
				return ((char *)&haystack[i]);
			j++;
		}
		i++;
	}
	return (0);
}

size_t strlen_s(const char* str) {
    size_t len = 0;
    while (*str) {
        ++len;
        ++str;
    }

    return len;
}

char* strncpy_s(char* dst, char* src, int n) {
	int	i = 0;
	while (i < n && src[i]) {
		dst[i] = src[i];
		i++;
	}

	while (i < n) {
		dst[i] = '\0';
		i++;
	}

	return dst;
}

int strncmp_s(char* str1, const char* str2, size_t n) {
    for (size_t i = 0; i < n; ++i) 
        if (str1[i] != str2[i] || str1[i] == '\0' || str2[i] == '\0') 
            return (unsigned char)str1[i] - (unsigned char)str2[i];
        
    return 0;
}

char* strcpy_s(char* dst, char* src) {
    if (strlen_s(src) <= 0) return NULL;

	int	i = 0;
	while (src[i]) {
		dst[i] = src[i];
		i++;
	}

	dst[i] = '\0';
    return dst;
}

void* __rawmemchr(const void* s, int c_in) {
  const unsigned char* char_ptr = NULL;
  const unsigned long int* longword_ptr = NULL;
  unsigned long int longword, magic_bits, charmask = 0;
  unsigned char c = 0;

  c = (unsigned char) c_in;

  /* Handle the first few characters by reading one character at a time.
     Do this until CHAR_PTR is aligned on a longword boundary.  */
  for (char_ptr = (const unsigned char *) s;
       ((unsigned long int) char_ptr & (sizeof (longword) - 1)) != 0;
       ++char_ptr)
    if (*char_ptr == c)
      return (void*) char_ptr;

  /* All these elucidatory comments refer to 4-byte longwords,
     but the theory applies equally well to 8-byte longwords.  */

  longword_ptr = (unsigned long int *) char_ptr;

#if LONG_MAX <= LONG_MAX_32_BITS
  magic_bits = 0x7efefeff;
#else
  magic_bits = ((unsigned long int) 0x7efefefe << 32) | 0xfefefeff;
#endif

  /* Set up a longword, each of whose bytes is C.  */
  charmask = c | (c << 8);
  charmask |= charmask << 16;
#if LONG_MAX > LONG_MAX_32_BITS
  charmask |= charmask << 32;
#endif

    while (1) {
        longword = *longword_ptr++ ^ charmask;

        /* Add MAGIC_BITS to LONGWORD.  */
        if ((((longword + magic_bits) ^ ~longword) & ~magic_bits) != 0) {
                const unsigned char *cp = (const unsigned char *) (longword_ptr - 1);

                if (cp[0] == c) return (void*)cp;
                if (cp[1] == c) return (void*)&cp[1];
                if (cp[2] == c) return (void*)&cp[2];
                if (cp[3] == c) return (void*)&cp[3];

                #if LONG_MAX > 2147483647
                    if (cp[4] == c) return (void*) &cp[4];
                    if (cp[5] == c) return (void*) &cp[5];
                    if (cp[6] == c) return (void*) &cp[6];
                    if (cp[7] == c) return (void*) &cp[7];
                #endif
            }
    }
}

char* strpbrk_s(char* s, char* accept) {
    while (*s != '\0') {
        char *a = accept;
        while (*a != '\0')
	        if (*a++ == *s)
	            return (char *) s;
            ++s;
    }

  return NULL;
}

size_t strspn_s(char* s, char* accept) {
    char* p = NULL;
    char* a = NULL;
    size_t count = 0;

    for (p = s; *p != '\0'; ++p) {
        for (a = accept; *a != '\0'; ++a)
            if (*p == *a)
                break;

        if (*a == '\0') return count;
        else ++count;
    }

    return count;
}

static char* olds;
char* strtok_s(char* string, char* delim){
    char* token;
    if (string == NULL) string = olds;

    string += strspn_s(string, delim);
    if (*string == '\0') {
        olds = string;
        return NULL;
    }

    token = string;
    string = strpbrk_s(token, delim);
    if (string == NULL) olds = __rawmemchr(token, '\0');
    else {
        *string = '\0';
        olds = string + 1;
    }

    return token;
}

char* strcat_s(char* dest, char* src) {
    strcpy_s(dest + strlen_s(dest), src);
    return dest;
}

char* strchr_s(char* str, char chr) {
    if (str == NULL) return NULL;
    while (*str) {
        if (*str == chr)
            return str;

        ++str;
    }

    return NULL;
}

int strcmp_s(char* firstStr, char* secondStr) {
    if (firstStr == NULL || secondStr == NULL) return -1;
    while (*firstStr && *secondStr && *firstStr == *secondStr) {
        ++firstStr;
        ++secondStr;
    }

    return (unsigned char)(*firstStr) - (unsigned char)(*secondStr);
}

int atoi_s(char *str) {
    int neg = 1;
    long long num = 0;
    size_t i = 0;

    while (*str == ' ') str++;
    if (*str == '-' || *str == '+') {
        neg = *str == '-' ? -1 : 1;
        str++;
    }

	while (*str >= '0' && *str <= '9' && *str) {
		num = num * 10 + (str[i] - 48);
        if (neg == 1 && num > INT_MAX) return INT_MAX;
        if (neg == -1 && -num < INT_MIN) return INT_MIN;
		str++;
	}
    
	return (num * neg);
}

char** copy_array2array(void* source, size_t elem_size, size_t count, size_t row_size) {
    char** temp_names = (char**)malloc_s(sizeof(char*) * count);
    if (!temp_names) return NULL;

    for (size_t i = 0; i < count; i++) {
        temp_names[i] = (char*)malloc_s(row_size);
        if (!temp_names[i]) {
            ARRAY_SOFT_FREE(temp_names, (int)count);
            return NULL;
        }

        memcpy_s(temp_names[i], (char*)source + i * elem_size, row_size);
    }

    return temp_names;
}
