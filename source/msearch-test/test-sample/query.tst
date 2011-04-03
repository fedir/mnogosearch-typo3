FIELDS=OFF;
SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag;
SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32, hops;
SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname;
SELECT u1.docsize,u2.docsize,u1.url,u2.url FROM url u1,url u2, links l WHERE u1.rec_id=l.ot AND u2.rec_id=l.k ORDER BY u1.docsize,u2.docsize;
SELECT l.word, l.intag, u1.docsize,u2.docsize,u1.url,u2.url FROM url u1,url u2, crossdict l WHERE u1.rec_id=l.ref_id AND u2.rec_id=l.url_id ORDER BY u1.docsize,u2.docsize, l.intag;
SELECT url FROM url WHERE url='http://site/';
