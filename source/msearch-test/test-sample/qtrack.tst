FIELDS=OFF;
SELECT qwords, found FROM qtrack WHERE rec_id>0 AND qtime>0 AND wtime>0 AND found>0 AND (ip<>'impossible' or ip is null);
