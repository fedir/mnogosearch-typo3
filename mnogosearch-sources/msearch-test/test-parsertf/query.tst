FIELDS=OFF;
SELECT url.seed,url.docsize,url.url,dict.intag,dict.word FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.seed,url.docsize,dict.intag;
SELECT seed,docsize,status,url FROM url ORDER BY seed,docsize,status;
SELECT url.seed,url.docsize,url.status,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.seed,url.docsize,url.status,urlinfo.sname;
