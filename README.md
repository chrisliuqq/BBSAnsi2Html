BBSAnsi2Html
============

A php extension to convert BBS ansi code to html.
這是一個將 BBS 的 Ansi 碼轉為 HTML 的 php extension.

Complie
============

```gcc -fpic -DCOMPILE_DL_FIRST_MODULE=1 -I/usr/local/include -I.
    -I/usr/include/php5 -I/usr/include/php5/Zend
    -I/usr/include/php5//main -I/usr/include/php5/TSRM
    -c -o BBSAnsi2Html.o BBSAnsi2Html.c```

```gcc -shared -L/usr/local/lib -rdynamic -o BBSAnsi2Html.so BBSAnsi2Html.o```

Setup
============

copy the complied so file to your php library folder.

```cp BBSAnsi2Html.so /usr/lib/php5/20060613+lfs/```

and add following config to your php.ini.

```extension=BBSAnsi2Html.so```

Usage
============
```
// $path 可能為 /home/bbs/brd/1/A1111111 這樣
ansi_to_html($path);

$fd = fopen ('/tmp/bbs2htm', "r");
$content = fread ($fd, filesize('/tmp/bbs2htm'));
fclose ($fd);

// 我個人偏好 utf8 所以加了 iconv 轉換
$content = iconv( 'Big5HKSCS','utf-8', $content);
```

Known problem
============

Because all converted html is put into /tmp/bbs2htm, so might cause some problem when mass view in one time.
It should be fixed with different file or memcache.