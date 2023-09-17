create database if not exists `gobang`;
use `gobang`;
create table if not exists `user`(
    `id` int primary key auto_increment,
    `username` varchar(64) unique key,
    `password` varchar(128),
    `score` int,
    `total_count` int,
    `win_count` int
);
insert into user values(null,'张三',password('2000'),100,0,0);
insert into user values(null,'李四',password('2001'),100,0,0);
insert into user values(null,'王五',password('2002'),100,0,0);
-- mysql -uroot -p < db.sql 导入数据