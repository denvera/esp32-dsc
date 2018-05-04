#include <string.h>

/*
 * strnstr - locate a substring in a fixed-size string.
 *
 * PARAMETERS:
 *   _CONST char *haystack - the string in which to search.
 *   _CONST char *needle   - the string which to search.
 *   int length            - the maximum 'haystack' string length.
 *
 * DESCRIPTION:
 *   The  strstr() function finds the first occurrence of the substring
 *   'needle' in the string 'haystack'. At most 'length' bytes are searched.
 *
 * RETURN:
 *   Returns a pointer to the beginning of substring, or NULL if substring
 *   was not found.
 */
char * strnstr(char *haystack,char *needle, int length)
{
  const char *max = haystack + length;

  if (*haystack == '\0')
    return *needle == '\0' ? (char *)haystack : (char *)NULL;

  while (haystack < max)
    {
      int i = 0;
      while (1)
        {
          if (needle[i] == '\0')
            return (char *)haystack;
          if (needle[i] != haystack[i])
            break;
          i += 1;
        }
      haystack += 1;
    }
  return (char *)NULL;
}

/*
 * Find the first occurrence of find in s, where the search is limited to the
 * first slen characters of s.
 */
// char *
// strnstr(const char *s, const char *find, size_t slen)
// {
// 	char c, sc;
// 	size_t len;
//
// 	if ((c = *find++) != '\0') {
// 		len = strlen(find);
// 		do {
// 			do {
// 				if (slen-- < 1 || (sc = *s++) == '\0')
// 					return (NULL);
// 			} while (sc != c);
// 			if (len > slen)
// 				return (NULL);
// 		} while (strncmp(s, find, len) != 0);
// 		s--;
// 	}
// 	return ((char *)s);
// }
