#include "boyer_moore.h"

void computeJump(char* pattern, int patternlen, int* jump)
{
	for(int i = 0; i < 300; i++)
		jump[i] = patternlen;
	for(int j = 0; j < patternlen - 1; j++)
		jump[pattern[j]] = patternlen - 1 - j;
}
int BoyerMooreHorspool(char* text, int textlen, char* pattern, int patternlen, int* jump)
{
	int i = 0, j, k;
	while(i <= textlen - patternlen)
	{
		j = patternlen - 1;
		k = i + patternlen - 1;
		while(j >= 0 && pattern[j] == text[k])
		{
			j--;
			k--;
		}
		if(j == -1)
			return i;
		i = i + jump[text[i + patternlen - 1]];
	}
	return -1;
}
