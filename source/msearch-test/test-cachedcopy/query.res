SQL>'FIELDS=OFF'
SQL>'SELECT seed, status, url FROM url ORDER BY seed'
12	200	http://site/?a=b&c=d
80	200	http://site/a.php
238	200	http://site/bug749.html
SQL>'SELECT url.seed,urlinfo.sval FROM url,urlinfo WHERE urlinfo.sname='CachedCopy' and url.rec_id=urlinfo.url_id ORDER BY url.seed'
12	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
238	eNqzySjJzbHjsknKT6m040qEAoUkKFBIhgIuG32IEht9sA4AGBgSpQ== aaaaaaaa bbbbbbbb cccccccc
SQL>