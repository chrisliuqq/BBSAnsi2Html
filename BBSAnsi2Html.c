/* include standard header */
#include "php.h"


/* ----------------------------------------------------- */
/* target : ANSI text to HTML tag                        */
/* author : yiting.bbs@bbs.cs.tku.edu.tw                 */
/* ----------------------------------------------------- */

typedef unsigned char uschar;   /* length = 1 */
typedef unsigned int usint;     /* length = 4 */

#define ANSI_TAG                27
#define TTLEN                   72
#define ANSILINELEN             512                     /* Maximum Screen width in chars */
#define LINE_HEADER             4               /* more.c bhttpd.c 檔頭有三列 */
#define QUOTE_CHAR1             '>'
#define QUOTE_CHAR2             ':'
#define STR_AUTHOR1             "作者:"
#define STR_AUTHOR2             "發信人:"
#define LEN_AUTHOR1             (sizeof(STR_AUTHOR1) - 1)
#define LEN_AUTHOR2             (sizeof(STR_AUTHOR2) - 1)


#define is_ansi(ch)             ((ch >= '0' && ch <= '9') || ch == ';' || ch == '[')
#define IS_ZHC_HI(x)            (x & 0x80)

#define HAVE_HYPERLINK          /* 處理超連結 */
#undef  HAVE_ANSIATTR           /* 很少用到而且IE不支援閃爍，乾脆不處理 :( */
#define HAVE_SAKURA             /* 櫻花日文自動轉Unicode */

#ifdef HAVE_ANSIATTR
#define ATTR_UNDER      0x1     /* 底線 */
#define ATTR_BLINK      0x2     /* 閃動 */
#define ATTR_ITALIC     0x4     /* 斜體 */

static int old_attr, now_attr;
#endif

static int old_color, now_color;
static char ansi_buf[1024];     /* ANSILINELEN * 4 */


#ifdef HAVE_HYPERLINK
static uschar *linkEnd = NULL;

static void
ansi_hyperlink(fpw, src)
FILE *fpw;
uschar *src;
{

	int ch;

	linkEnd = src;

	fputs("<a class=PRE target=_blank href=", fpw);
	while (ch = *linkEnd)
	{
		if (ch < '#' || ch == '<' || ch == '>' || ch > '~')
			break;
		fputc(ch, fpw);
		linkEnd++;
	}

	fputc('>', fpw);

}
#endif


#ifdef HAVE_SAKURA
static int
sakura2unicode(code)
int code;
{
	if (code > 0xC6DD && code < 0xC7F3)
	{
		if (code > 0xC7A0)
			code -= 38665;
		else if (code > 0xC700)
			code -= 38631;
		else if (code > 0xC6E6)
			code -= 38566;
		else if (code == 0xC6E3)
			return 0x30FC;
		else
			code -= 38619;
		if (code > 0x3093)
			code += 13;
		return code;
	}
	return 0;
}
#endif


static int
ansi_remove(psrc)
uschar **psrc;
{
	uschar *src = *psrc;
	int ch = *src;

	while (is_ansi(ch))
		ch = *(++src);

	if (ch && ch != '\n')
		ch = *(++src);

	*psrc = src;
	return ch;
}


static int
ansi_color(psrc)
uschar **psrc;
{
	uschar *src, *ptr;
	int ch, value;
	int color = old_color;
#ifdef HAVE_ANSIATTR
	int attr = old_attr;
#endif
	uschar *cptr = (uschar *) & color;

	src = ptr = (*psrc) + 1;

	ch = *src;
	while (ch)
	{
		if (ch == ';' || ch == 'm')
		{
			*src = '\0';
			value = atoi(ptr);
			ptr = src + 1;
			if (value == 0)
			{
				color = 0x00003740;

#ifdef HAVE_ANSIATTR
				attr = 0;
#endif
			}
			else if (value >= 30 && value <= 37)
				cptr[1] = value + 18;
			else if (value >= 40 && value <= 47)
				cptr[0] = value + 24;
			else if (value == 1)
				cptr[2] = 1;

#ifdef HAVE_ANSIATTR
			else if (value == 4)
				attr |= ATTR_UNDER;
			else if (value == 5)
				attr |= ATTR_BLINK;
      else if (value == 7)      /* 反白的效果用斜體來代替 */
			attr |= ATTR_ITALIC;
#endif

			if (ch == 'm')
			{
				now_color = color;

#ifdef HAVE_ANSIATTR
				now_attr = attr;
#endif

				ch = *(++src);
				break;
			}
		}
		else if (ch < '0' || ch > '9')
		{
			ch = *(++src);
			break;
		}
		ch = *(++src);
	}

	*psrc = src;
	return ch;
}


static void
ansi_tag(fpw)
FILE *fpw;
{
#ifdef HAVE_ANSIATTR
  /* 屬性不同才需要印出 */
	if (!(now_attr & ATTR_ITALIC) && (old_attr & ATTR_ITALIC))
	{
		fputs("</I>", fpw);
	}
	if (!(now_attr & ATTR_UNDER) && (old_attr & ATTR_UNDER))
	{
		fputs("</U>", fpw);
	}
	if (!(now_attr & ATTR_BLINK) && (old_attr & ATTR_BLINK))
	{
		fputs("</BLINK>", fpw);
	}
#endif

  /* 顏色不同才需要印出 */
	if (old_color != now_color)
	{
		fprintf(fpw, "</font><font class=A%05X>", now_color);
		old_color = now_color;
	}

#ifdef HAVE_ANSIATTR
  /* 屬性不同才需要印出 */
	if (oldattr != attr)
	{
		if ((now_attr & ATTR_ITALIC) && !(old_attr & ATTR_ITALIC))
		{
			fputs("<I>", fpw);
		}
		if ((now_attr & ATTR_UNDER) && !(old_attr & ATTR_UNDER))
		{
			fputs("<U>", fpw);
		}
		if ((now_attr & ATTR_BLINK) && !(old_attr & ATTR_BLINK))
		{
			fputs("<BLINK>", fpw);
		}
		old_attr = now_attr;
	}
#endif
}


static void
ansi_html(fpw, src)
FILE *fpw;
uschar *src;
{
	int ch1, ch2;
	int has_ansi = 0;

#ifdef HAVE_SAKURA
	int scode;
#endif

	ch2 = *src;
	while (ch2)
	{
		ch1 = ch2;
		ch2 = *(++src);
		if (IS_ZHC_HI(ch1))
		{
			while (ch2 == ANSI_TAG)
			{
        if (*(++src) == '[')    /* 顏色 */
				{
					ch2 = ansi_color(&src);
					has_ansi = 1;
				}
        else                    /* 其他直接刪除 */
				ch2 = ansi_remove(&src);
			}
			if (ch2)
			{
        if (ch2 < ' ')          /* 怕出現\n */
				fputc(ch2, fpw);
#ifdef HAVE_SAKURA
				else if (scode = sakura2unicode((ch1 << 8) | ch2))
					fprintf(fpw, "&#%d;", scode);
#endif
				else
				{
					fputc(ch1, fpw);
					fputc(ch2, fpw);
				}
				ch2 = *(++src);
			}
			if (has_ansi)
			{
				has_ansi = 0;
				if (ch2 != ANSI_TAG)
					ansi_tag(fpw);
			}
			continue;
		}
		else if (ch1 == ANSI_TAG)
		{
			do
			{
        if (ch2 == '[')         /* 顏色 */
				ch2 = ansi_color(&src);
        else if (ch2 == '*')    /* 控制碼 */
				fputc('*', fpw);
        else                    /* 其他直接刪除 */
				ch2 = ansi_remove(&src);
			} while (ch2 == ANSI_TAG && (ch2 = *(++src)));
			ansi_tag(fpw);
			continue;
		}
    /* 剩下的字元做html轉換 */

		if (ch1 == '<')
		{
			fputs("&lt;", fpw);
		}
		else if (ch1 == '>')
		{
			fputs("&gt;", fpw);
		}
		else if (ch1 == '&')
		{
			fputc(ch1, fpw);
      if (ch2 == '#')           /* Unicode字元不轉換 */
			{
				fputc(ch2, fpw);
				ch2 = *(++src);
			}
			else if (ch2 >= 'A' && ch2 <= 'z')
			{
				fputs("amp;", fpw);
				fputc(ch2, fpw);
				ch2 = *(++src);
			}
		}
#ifdef HAVE_HYPERLINK
    else if (linkEnd)           /* 處理超連結 */
		{
			fputc(ch1, fpw);
			if (linkEnd <= src)
			{
				fputs("</a>", fpw);
				linkEnd = NULL;
			}
		}
#endif
		else
		{
#ifdef HAVE_HYPERLINK
      /* 其他的自己加吧 :) */
  /*
      if (!str_ncmp(src - 1, "http://", 7))
        ansi_hyperlink(fpw, src - 1);

      else if (!str_ncmp(src - 1, "telnet://", 9))
        ansi_hyperlink(fpw, src - 1);
        */
#endif

    fputc(ch1, fpw);
}
}
}


static char *
str_html(src, len)
uschar *src;
int len;
{
	int in_chi, ch;
	uschar *dst = ansi_buf, *end = src + len;

	ch = *src;
	while (ch && src < end)
	{
		if (IS_ZHC_HI(ch))
		{
			in_chi = *(++src);
			while (in_chi == ANSI_TAG)
			{
				src++;
				in_chi = ansi_remove(&src);
			}

			if (in_chi)
			{
        if (in_chi < ' ')       /* 可能只有半個字，前半部就不要了 */
				*dst++ = in_chi;
#ifdef HAVE_SAKURA
				else if (len = sakura2unicode((ch << 8) + in_chi))
				{
          sprintf(dst, "&#%d;", len);   /* 12291~12540 */
					dst += 8;
				}
#endif
				else
				{
					*dst++ = ch;
					*dst++ = in_chi;
				}
			}
			else
				break;
		}
		else if (ch == ANSI_TAG)
		{
			src++;
			ch = ansi_remove(&src);
			continue;
		}
		else if (ch == '<')
		{
			strcpy(dst, "&lt;");
			dst += 4;
		}
		else if (ch == '>')
		{
			strcpy(dst, "&gt;");
			dst += 4;
		}
		else if (ch == '&')
		{
			ch = *(++src);
			if (ch == '#')
			{
        if ((uschar *) strchr(src + 1, ';') >= end)     /* 可能會不是或長度沒超過 */
				break;
				*dst++ = '&';
				*dst++ = '#';
			}
			else
			{
				strcpy(dst, "&amp;");
				dst += 5;
				continue;
			}
		}
		else
			*dst++ = ch;
		ch = *(++src);
	}

	*dst = '\0';
	return ansi_buf;
}


static int
ansi_quote(fpw, src)            /* 如果是引言，就略過所有的 ANSI 碼 */
FILE *fpw;
uschar *src;
{
	int ch1, ch2;

	ch1 = src[0];
	ch2 = src[1];
  if (ch2 == ' ' && (ch1 == QUOTE_CHAR1 || ch1 == QUOTE_CHAR2)) /* 引言 */
	{
		ch2 = src[2];
    if (ch2 == QUOTE_CHAR1 || ch2 == QUOTE_CHAR2)       /* 引用一層/二層不同顏色 */
		now_color = 0x00003340;
		else
			now_color = 0x00003640;
	}
  else if (ch1 == '\241' && ch2 == '\260')      /* ※ 引言者 */
	{
		now_color = 0x00013640;
	}
	else
	{
		ansi_tag(fpw);
    return 0;                   /* 不是引言 */
	}

	ansi_tag(fpw);
	fputs(str_html(src, ANSILINELEN), fpw);
	now_color = 0x00003740;
	return 1;
}


static void
txt2htm(fpw, fp)
FILE *fpw;
FILE *fp;
{
	static const char header1[LINE_HEADER][LEN_AUTHOR1] = {"作者", "標題", "時間"};
	static const char header2[LINE_HEADER][LEN_AUTHOR2] = {"發信人", "標  題", "發信站"};
	int i;
	char *headvalue, *pbrd, *board;
	char buf[ANSILINELEN];

	fputs("<table width=760 cellspacing=0 cellpadding=0 border=0>\n", fpw);
  /* 處理檔頭 */
	for (i = 0; i < LINE_HEADER; i++)
	{
    if (!fgets(buf, ANSILINELEN, fp))   /* 雖然連檔頭都還沒印完，但是檔案已經結束，直接離開 */
		{
			fputs("</table>\n", fpw);
			return;
		}

    if (memcmp(buf, header1[i], LEN_AUTHOR1 - 1) && memcmp(buf, header2[i], LEN_AUTHOR2 - 1))   /* 不是檔頭 */
		break;

    /* 作者/看板 檔頭有二欄，特別處理 */
		if (i == 0 && ((pbrd = strstr(buf, "看板:")) || (pbrd = strstr(buf, "站內:"))))
		{
			if (board = strchr(pbrd, '\n'))
				*board = '\0';
			board = pbrd + 6;
			pbrd[-1] = '\0';
			pbrd[4] = '\0';
		}

		if (!(headvalue = strchr(buf, ':')))
			break;

		fprintf(fpw, "<tr>\n"
			"  <td align=center width=10%% class=A03447>%s</td>\n", header1[i]);

		str_html(headvalue + 2, TTLEN);
		if (i == 0 && pbrd)
		{
			fprintf(fpw, "  <td width=60%% class=A03744>&nbsp;%s</td>\n"
				"  <td align=center width=10%% class=A03447>%s</td>\n"
				"  <td width=20%% class=A03744>&nbsp;%s</td>\n</tr>\n",
				ansi_buf, pbrd, board);
		}
		else
		{
			fputs("  <td width=90% colspan=3 class=A03744>&nbsp;", fpw);
			fputs(ansi_buf, fpw);
			fputs("</td>\n</tr>\n", fpw);
		}
	}

	fputs("<tr>\n"
		"<td colspan=4><pre><font class=A03740>", fpw);

	old_color = now_color = 0x00003740;

#ifdef HAVE_ANSIATTR
	old_attr = now_attr = 0;
#endif

  if (i >= LINE_HEADER)         /* 最後一行是檔頭 */
	fgets(buf, ANSILINELEN, fp);

  /* 處理內文 */

	do
	{
		if (!ansi_quote(fpw, buf))
			ansi_html(fpw, buf);
	} while (fgets(buf, ANSILINELEN, fp));

	fputs("</font></pre></td>\n</table>\n", fpw);
}


static void
out_article(fpw, fpath)
FILE *fpw;
char *fpath;
{
	FILE *fp;

	if (fp = fopen(fpath, "r"))
	{
		txt2htm(fpw, fp);
		fclose(fp);
	}
}


/* ----------------------------------------------------- */
/* target : ANSI text to HTML tag PHP Extension                  */
/* author : yiting.bbs@bbs.cs.tku.edu.tw                 */
/* ----------------------------------------------------- */


/* declaration of functions to be exported */
ZEND_FUNCTION(ansi_to_html);

/* compiled function list so Zend knows what's in this module */
zend_function_entry ansitohtml_functions[] =
{
	ZEND_FE(ansi_to_html, NULL)
	{NULL, NULL, NULL}
};

/* compiled module information */
zend_module_entry ansitohtml_module_entry =
{
	STANDARD_MODULE_HEADER,
	"BBS ANSI to HTML",
	ansitohtml_functions,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NO_VERSION_YET,
	STANDARD_MODULE_PROPERTIES
};

/* implement standard "stub" routine to introduce ourselves to Zend */
#if COMPILE_DL_FIRST_MODULE
ZEND_GET_MODULE(ansitohtml)
#endif

/* implement function that is meant to be made available to PHP */
ZEND_FUNCTION(ansi_to_html)
{

        //if(ZEND_NUM_ARGS() != 1) WRONG_PARAM_COUNT;

        /* Gets a long, a string and its length, and a zval. */
	char *s;
	zval *param;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &s, &param) == FAILURE) {
		return;
}

FILE *fpw;

if (fpw = fopen("/tmp/bbs2htm", "w+"))
{
                //FILE *fp = fopen(s, "r");
	out_article(fpw, s);

	fclose(fpw);
}
RETURN_STRING(s, 1);

}