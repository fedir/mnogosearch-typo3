FIELDS=OFF;
SELECT dict.word,dict.intag,url.seed,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.seed,url.crc32,dict.intag;
SELECT status, docsize, hops, seed, crc32, url FROM url ORDER BY status, seed, crc32;
SELECT url.status,url.seed,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id AND sname<>'CachedCopy' ORDER BY url.status,url.seed,url.crc32,urlinfo.sname;
