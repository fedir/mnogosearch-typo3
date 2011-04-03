FIELDS=OFF;
-- Fixme: sections in some databases are with space characters in the end.
--SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32;
--SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname;

colflags4=1;

SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict00 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict01 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict02 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict03 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict04 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict05 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict06 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict07 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict08 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict09 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict0A d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict0B d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict0C d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict0D d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict0E d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dict0F d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;

SELECT u.crc32,u.url,d.secno,d.word,d.intag FROM dictB7 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.secno,d.word;
