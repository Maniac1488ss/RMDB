create table t ( id int , t_name char (500));
begin;
insert into t values(10000,'');
insert into t values(9999,'');
insert into t values(9998,'');
insert into t values(9997,'');
insert into t values(9996,'');
commit ;
begin ;
insert into t values(9995,'');
insert into t values(9994,'');
insert into t values(9993,'');
insert into t values(9992,'');
insert into t values(9991,'');
abort ;
begin ;
insert into t values(9990,'');
insert into t values(9989,'');
insert into t values(9988,'');
insert into t values(9987,'');
insert into t values(9986,'');
abort ;
begin ;
insert into t values(9985,'');
insert into t values(9984,'');
insert into t values(9983,'');
insert into t values(9982,'');
insert into t values(9981,'');
insert into t values(9980,'');
commit ;
crash