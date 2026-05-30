/* ==================================================================
   ДЕМОНСТРАЦИОННЫЙ СЦЕНАРИЙ ДЛЯ ЗАЩИТЫ (вариант 2, индекс B+-tree)

   Запуск:  cwdb examples/defense_demo.sql
   Каждая команда снабжена пояснением и ожидаемым выводом — можно
   зачитывать вслух. Успех = OK, выборка = JSON-массив, ошибка = ERROR.
   Комментарии поддерживаются в стилях  //  и  --  и блочными.
   ================================================================== */

/* ----- 0. УРОВЕНЬ СИСТЕМЫ И БАЗЫ ДАННЫХ ------------------------- */
-- Удаляем базу с прошлого запуска. Если её нет — это тоже не ошибка.
DROP DATABASE demo;            -- ожидаем: OK
-- Создаём новую базу (каталог data/demo).
CREATE DATABASE demo;          -- ожидаем: OK
-- Делаем её активной, чтобы дальше не писать demo.students каждый раз.
USE demo;                      -- ожидаем: OK

/* ----- 1. DDL: создаём таблицу со всеми модификаторами ---------- */
-- id      — INDEXED: уникальный, не NULL, по нему строится B+-tree.
-- name    — NOT_NULL: обязателен.
-- faculty — DEFAULT "IU7": подставится, если значение не передать.
-- age     — обычный int, может быть NULL.
CREATE TABLE students (
  id         int    INDEXED,
  name       string NOT_NULL,
  faculty    string DEFAULT "IU7",
  age        int
);                             -- ожидаем: OK

/* ----- 2. INSERT: три формы вставки ----------------------------- */
-- 2a. Все столбцы заданы явно.
INSERT INTO students (id, name, faculty, age) VALUE (1, "Ivan", "IU8", 19);
                               -- ожидаем: OK
-- 2b. Частичная вставка: faculty не задан -> DEFAULT "IU7"; age -> NULL.
INSERT INTO students (id, name) VALUE (2, "Anna"), (3, "Petr");
                               -- ожидаем: OK (две строки за один запрос)
-- 2c. Многострочная вставка одним запросом.
INSERT INTO students (id, name, age) VALUE (4, "Olga", 22), (5, "Lev", 30);
                               -- ожидаем: OK

/* ----- 3. SELECT: вывод результата в JSON ----------------------- */
-- Все строки и столбцы. Обрати внимание: у Anna и Petr faculty="IU7"
-- (сработал DEFAULT), а age=null (пропущенный столбец стал NULL).
SELECT * FROM students;
-- ожидаем 5 объектов; Ivan(IU8,19), Anna(IU7,null), Petr(IU7,null),
--                     Olga(IU7,22), Lev(IU7,30)

-- Перечисление столбцов и переименование через AS (name -> who).
SELECT (id, name AS who, age) FROM students;
-- ожидаем те же 5 строк, но поле называется "who"

/* ----- 4. ПОИСК ПО ИНДЕКСУ B+-tree (целочисленный ключ) --------- */
-- Точечный поиск "==" по индексированному столбцу идёт через B+-tree
-- (метод find, спуск по дереву за O(log n)), а не перебором строк.
SELECT * FROM students WHERE id == 3;        -- ожидаем: [ Petr ]
-- Неравенство.
SELECT (id) FROM students WHERE id != 1;     -- ожидаем: [2,3,4,5]
-- Диапазонные операции идут через B+-tree (range + связный список листьев).
SELECT (id) FROM students WHERE id < 3;      -- ожидаем: [1,2]
SELECT (id) FROM students WHERE id <= 3;     -- ожидаем: [1,2,3]
SELECT (id) FROM students WHERE id > 3;      -- ожидаем: [4,5]
SELECT (id) FROM students WHERE id >= 3;     -- ожидаем: [3,4,5]

/* ----- 5. СТРОКИ: лексикографическое сравнение ------------------ */
SELECT (name) FROM students WHERE name == "Anna";  -- ожидаем: [ Anna ]
SELECT (name) FROM students WHERE name <  "L";     -- ожидаем: [ Ivan, Anna ]
SELECT (name) FROM students WHERE name >= "L";     -- ожидаем: [ Petr, Olga, Lev ]

/* ----- 6. BETWEEN — полуинтервал [a, b) ------------------------ */
-- Граница b НЕ включается: 2..5 даёт 2,3,4 (без 5).
SELECT (id) FROM students WHERE id BETWEEN 2 AND 5;   -- ожидаем: [2,3,4]

/* ----- 7. LIKE — регулярные выражения -------------------------- */
SELECT (name) FROM students WHERE name LIKE "A.*";    -- начинается с A -> [ Anna ]
SELECT (name) FROM students WHERE name LIKE "[OL].*"; -- O или L -> [ Olga, Lev ]

/* ----- 8. Составные условия: AND / OR / скобки ------------------ */
-- AND связывает сильнее OR; скобки меняют приоритет.
SELECT (id, name) FROM students WHERE age >= 20 AND age < 30;   -- -> [ Olga ]
SELECT (id, name) FROM students WHERE id == 1 OR id == 5;       -- -> [ Ivan, Lev ]
SELECT (id, name) FROM students WHERE (id < 3 OR id > 4) AND name LIKE ".*v.*";
-- id из {1,2,5}, имя содержит "v" -> [ Ivan, Lev ]

/* ----- 9. UPDATE ----------------------------------------------- */
-- Ставим возраст 25 всем, чьё имя начинается на A (это Anna).
UPDATE students SET age = 25 WHERE name LIKE "A.*";   -- ожидаем: OK
SELECT * FROM students WHERE name == "Anna";          -- ожидаем: Anna, age=25

/* ----- 10. Агрегатные функции SUM / COUNT / AVG ---------------- */
-- Считаются по подходящим строкам. age сейчас: 19,25,null,22,30.
-- COUNT=4 (строк с числовым age), SUM=96, AVG=96/4=24.
SELECT (COUNT(id) AS total, SUM(age) AS sum_age, AVG(age) AS avg_age)
  FROM students WHERE age >= 0;
-- ожидаем: [{ total:4, sum_age:96, avg_age:24 }]

/* ----- 11. Квалифицированное имя database.table ---------------- */
-- Можно обращаться без USE, указав базу явно.
SELECT * FROM demo.students WHERE id == 1;            -- ожидаем: [ Ivan ]

/* ----- 12. ВАЛИДАЦИЯ: программа НЕ падает, а возвращает ERROR ---- */
-- Здесь специально показываем обработку ошибок: каждая строка ниже
-- должна вернуть информативное ERROR, а выполнение продолжится.
INSERT INTO students (id, name) VALUE (1, "Dup");
-- ERROR: INDEXED column is not unique: id   (дубль ключа)
INSERT INTO students (id) VALUE (99);
-- ERROR: column cannot be NULL: name        (нарушен NOT_NULL)
INSERT INTO students (id, age) VALUE (7, "abc");
-- ERROR: expected int value: "abc"          (строка в int-столбец)
SELECT (unknown_col) FROM students;
-- ERROR: unknown column: unknown_col        (нет такого столбца)
SELECT * FROM no_such_table;
-- ERROR: table does not exist: no_such_table
SeLeCt * FROM students;
-- ERROR: mixed-case keyword is not allowed: SeLeCt  (смешанный регистр)

/* ----- 13. DELETE ---------------------------------------------- */
DELETE FROM students WHERE id == 5;            -- удаляем Lev, ожидаем: OK
SELECT (id) FROM students;                     -- ожидаем: [1,2,3,4]

/* ----- 14. Телеметрия (задание 8) ------------------------------ */
-- JSON с метриками: текущий RPS, средний/макс RPS за 10 мин, среднее
-- время обработки за 10 с, доля ошибок за 1 мин, размер окна событий.
-- error_rate ~0.167 = 6 наших ошибок из секции 12 на ~36 запросов.
SHOW TELEMETRY;
-- ожидаем: {"current_rps":..,"avg_rps_10m":..,...,"error_rate_1m":0.167,..}

/* ----- 15. Темпоральная персистентность REVERT (задание 1) ------
   REVERT восстанавливает таблицу из ближайшего снимка с меткой <=
   указанной (метки лежат в data/demo/_db_history.log). Формат метки:
   yyyy.mm.dd-hh:mm:ss.msmsms. В батче точную метку взять неоткуда,
   поэтому показываем два края поведения; содержательный откат —
   интерактивно, см. examples/defense_guide.md.                     */
REVERT students 2026.05.30;
-- ERROR: invalid timestamp format          (метка не того формата)
REVERT students 2099.12.31-23:59:59.999;
-- OK: метка в будущем -> берётся последний снимок (состояние не меняется)

/* ----- 16. DROP TABLE (на отдельной временной таблице) --------- */
-- students намеренно НЕ трогаем, чтобы после прогона показать файл
-- индекса data/demo/students/index_id.bpt и историю снимков.
CREATE TABLE tmp (id int INDEXED);             -- ожидаем: OK
INSERT INTO tmp (id) VALUE (1);                -- ожидаем: OK
DROP TABLE tmp;                                -- удаляем таблицу, ожидаем: OK

/* ----- 17. Завершение ------------------------------------------ */
SELECT * FROM students;     -- итог: Ivan, Anna(age 25), Petr, Olga (4 строки)
EXIT;                       -- завершение сеанса; в батче выводится BYE
