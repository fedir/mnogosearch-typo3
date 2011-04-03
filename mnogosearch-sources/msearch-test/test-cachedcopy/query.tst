FIELDS=OFF;
SELECT seed, status, url FROM url ORDER BY seed;
SELECT url.seed,urlinfo.sval FROM url,urlinfo WHERE urlinfo.sname='CachedCopy' and url.rec_id=urlinfo.url_id ORDER BY url.seed;
