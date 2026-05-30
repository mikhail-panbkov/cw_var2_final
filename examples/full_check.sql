-- 1. CREATE/DROP/USE DATABASE
DROP DATABASE check_db;
CREATE DATABASE check_db;
USE check_db;

-- 2. CREATE TABLE: int, string, NOT_NULL, INDEXED, DEFAULT
CREATE TABLE users (
  id int INDEXED,
  name string NOT_NULL,
  group_name string DEFAULT "IU7",
  age int
);

-- 3. INSERT: full columns
INSERT INTO users (id, name, group_name, age) VALUE (1, "Ivan", "IU8", 19);

-- 4. INSERT: partial columns (group_name should get DEFAULT, age should be NULL)
INSERT INTO users (id, name) VALUE (2, "Anna"), (3, "Petr");

-- 5. INSERT: multi-row in one statement
INSERT INTO users (id, name, age) VALUE (4, "Olga", 22), (5, "Lev", 30);

-- 6. SELECT * — all rows, JSON output
SELECT * FROM users;

-- 7. SELECT with explicit columns + alias
SELECT (id, name AS who, age) FROM users;

-- 8. Operator == on int (uses B+-tree index)
SELECT * FROM users WHERE id == 3;

-- 9. Operator != on int
SELECT (id) FROM users WHERE id != 1;

-- 10. Operators <, <=, >, >= on int (uses B+-tree for indexed col)
SELECT (id) FROM users WHERE id < 3;
SELECT (id) FROM users WHERE id <= 3;
SELECT (id) FROM users WHERE id > 3;
SELECT (id) FROM users WHERE id >= 3;

-- 11. Operators on string (lexicographic)
SELECT (name) FROM users WHERE name == "Anna";
SELECT (name) FROM users WHERE name < "L";
SELECT (name) FROM users WHERE name >= "L";

-- 12. BETWEEN [a, b)  (half-open per task)
SELECT (id) FROM users WHERE id BETWEEN 2 AND 5;

-- 13. LIKE with regex
SELECT (name) FROM users WHERE name LIKE "A.*";
SELECT (name) FROM users WHERE name LIKE "[OL].*";

-- 14. AND / OR / parentheses
SELECT (id, name) FROM users WHERE age >= 20 AND age < 30;
SELECT (id, name) FROM users WHERE id == 1 OR id == 5;
SELECT (id, name) FROM users WHERE (id < 3 OR id > 4) AND name LIKE ".*v.*";

-- 15. UPDATE with WHERE
UPDATE users SET age = 25 WHERE name LIKE "A.*";
SELECT * FROM users WHERE name == "Anna";

-- 16. DELETE with WHERE
DELETE FROM users WHERE id == 5;
SELECT (id) FROM users;

-- 17. Aggregates SUM / COUNT / AVG
SELECT (COUNT(id) AS total, SUM(age) AS s, AVG(age) AS a) FROM users WHERE age >= 0;

-- 18. Qualified database.table name
SELECT * FROM check_db.users WHERE id == 1;

-- 19. INDEXED uniqueness violation must be rejected
INSERT INTO users (id, name) VALUE (1, "Dup");

-- 20. NOT_NULL violation must be rejected
INSERT INTO users (id) VALUE (99);

-- 21. NULL handling in WHERE
SELECT (id, age) FROM users WHERE age == 25;

-- 22. Telemetry
SHOW TELEMETRY;

-- 23. DROP TABLE
DROP TABLE users;

-- 24. End session
EXIT;
