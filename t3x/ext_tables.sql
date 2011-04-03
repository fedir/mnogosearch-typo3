#
# Table structure for table 'tx_mnogosearch_indexconfig'
#
CREATE TABLE tx_mnogosearch_indexconfig (
	uid int(11) NOT NULL auto_increment,
	pid int(11) DEFAULT '0' NOT NULL,
	tstamp int(11) DEFAULT '0' NOT NULL,
	crdate int(11) DEFAULT '0' NOT NULL,
	cruser_id int(11) DEFAULT '0' NOT NULL,
	sorting int(11) DEFAULT '0' NOT NULL,
	title varchar(255) DEFAULT '' NOT NULL,
	tx_mnogosearch_type int(11) DEFAULT '0' NOT NULL,
	tx_mnogosearch_url varchar(255) DEFAULT '' NOT NULL,
	tx_mnogosearch_method int(11) DEFAULT '-1' NOT NULL,
	tx_mnogosearch_subsection int(11) DEFAULT '-1' NOT NULL,
	tx_mnogosearch_cmptype int(11) DEFAULT '-1' NOT NULL,
	tx_mnogosearch_cmpoptions int(11) DEFAULT '0' NOT NULL,
	tx_mnogosearch_period varchar(32) DEFAULT '' NOT NULL,
	tx_mnogosearch_additional_config text,

	tx_mnogosearch_table varchar(255) DEFAULT '' NOT NULL,
	tx_mnogosearch_title_field varchar(255) DEFAULT '' NOT NULL,
	tx_mnogosearch_body_field varchar(255) DEFAULT '' NOT NULL,
	tx_mnogosearch_lastmod_field varchar(255) DEFAULT '' NOT NULL,
	tx_mnogosearch_url_parameters text,
	tx_mnogosearch_display_pid int(11) DEFAULT '0' NOT NULL,
	tx_mnogosearch_pid_only text,
	
	user_groups text,

	PRIMARY KEY (uid),
	KEY tx_mnogosearch_table (tx_mnogosearch_table(64),sorting)
);

CREATE TABLE tx_mnogosearch_urllog (
	uid int(11) NOT NULL auto_increment,
	pid int(11) DEFAULT '0' NOT NULL,
	tstamp int(11) DEFAULT '0' NOT NULL,
	crdate int(11) DEFAULT '0' NOT NULL,
	cruser_id int(11) DEFAULT '0' NOT NULL,
	tx_mnogosearch_url varchar(255) DEFAULT '' NOT NULL,

	PRIMARY KEY (uid),
	KEY tx_mnogosearch_url (tx_mnogosearch_url(100))
);
