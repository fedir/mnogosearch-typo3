FIELDS=OFF;
select seed, status, crc32, url from url order by crc32, status, seed;
