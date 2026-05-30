/* ============================================================
   ДЕМОНСТРАЦИОННЫЙ СЦЕНАРИЙ ДЛЯ ЗАЩИТЫ (вариант 2, индекс B+-tree)
   Запуск:  cwdb examples/defense_demo.sql
   Комментарии поддерживаются в стилях  //  и  --  и блочными.
   ============================================================ */

/* ----- 0. ПОДГОТОВКА: уровень системы и базы данных ------------ */
DROP DATABASE demo;            -- очистка прошлого запуска (нет БД — тоже OK)
CREATE DATABASE demo;          -- создание базы
USE demo;                      -- выбор активного контекста

/* ----- 1. DDL: таблица со всеми модификаторами целостности ----- */
//   int / string, INDEXED (уникально + NOT NULL + B+-tree),
//   NOT_NULL, DEFAULT
CREATE TABLE students (
  id         int    INDEXED,
  name       string NOT_NULL,
  faculty    string DEFAULT "IU7",
  age        int
);

/* ----- 2. INSERT: разные формы вставки ------------------------- */
-- 2a. все столбцы
INSERT INTO students (id, name, faculty, age) VALUE (1, "Ivan", "IU8", 19);
-- 2b. частичная вставка: faculty получит DEFAULT "IU7", age станет NULL
INSERT INTO students (id, name) VALUE (2, "Anna"), (3, "Petr");
-- 2c. многострочная вставка одним запросом
INSERT INTO students (id, name, age) VALUE (4, "Olga", 22), (5, "Lev", 30);

/* ----- 3. SELECT: вывод в JSON -------------------------------- */
-- 3a. все строки и столбцы
SELECT * FROM students;
-- 3b. перечисление столбцов + алиас AS
SELECT (id, name AS who, age) FROM students;

/* ----- 4. ПОИСК ПО ИНДЕКСУ B+-tree (целочисленный ключ) -------- */
-- точечный поиск == идёт через B+-tree (find)
SELECT * FROM students WHERE id == 3;
-- неравенство
SELECT (id) FROM students WHERE id != 1;
-- диапазонные операции идут через B+-tree (range + связный список листьев)
SELECT (id) FROM students WHERE id < 3;
SELECT (id) FROM students WHERE id <= 3;
SELECT (id) FROM students WHERE id > 3;
SELECT (id) FROM students WHERE id >= 3;

/* ----- 5. СТРОКИ: лексикографическое сравнение ----------------- */
SELECT (name) FROM students WHERE name == "Anna";
SELECT (name) FROM students WHERE name <  "L";
SELECT (name) FROM students WHERE name >= "L";

/* ----- 6. BETWEEN — полуинтервал [a, b) ----------------------- */
SELECT (id) FROM students WHERE id BETWEEN 2 AND 5;   -- вернёт 2,3,4 (без 5)

/* ----- 7. LIKE — регулярные выражения ------------------------- */
SELECT (name) FROM students WHERE name LIKE "A.*";     -- начинается с A
SELECT (name) FROM students WHERE name LIKE "[OL].*";  -- начинается с O или L

/* ----- 8. Составные условия: AND / OR / скобки ---------------- */
SELECT (id, name) FROM students WHERE age >= 20 AND age < 30;
SELECT (id, name) FROM students WHERE id == 1 OR id == 5;
SELECT (id, name) FROM students WHERE (id < 3 OR id > 4) AND name LIKE ".*v.*";

/* ----- 9. UPDATE ---------------------------------------------- */
UPDATE students SET age = 25 WHERE name LIKE "A.*";
SELECT * FROM students WHERE name == "Anna";

/* ----- 10. Агрегатные функции SUM / COUNT / AVG --------------- */
SELECT (COUNT(id) AS total, SUM(age) AS sum_age, AVG(age) AS avg_age)
  FROM students WHERE age >= 0;

/* ----- 11. Квалифицированное имя database.table --------------- */
SELECT * FROM demo.students WHERE id == 1;

/* ----- 12. ВАЛИДАЦИЯ: ошибки должны быть информативными ------- */
//   программа не падает, а возвращает строку, начинающуюся с ERROR
INSERT INTO students (id, name) VALUE (1, "Dup");   -- нарушение уникальности INDEXED
INSERT INTO students (id) VALUE (99);               -- нарушение NOT_NULL (name)
INSERT INTO students (id, age) VALUE (7, "abc");    -- несоответствие типа (age int)
SELECT (unknown_col) FROM students;                 -- неизвестный столбец
SELECT * FROM no_such_table;                        -- нет таблицы
SeLeCt * FROM students;                             -- смешанный регистр ключевого слова

/* ----- 13. DELETE -------------------------------------------- */
DELETE FROM students WHERE id == 5;
SELECT (id) FROM students;

/* ----- 14. Телеметрия (RPS, время обработки, error rate) ------ */
SHOW TELEMETRY;

/* ----- 15. Темпоральная персистентность (REVERT) -------------
   REVERT восстанавливает таблицу из ближайшего снимка <= метки.
   Метка формата yyyy.mm.dd-hh:mm:ss.msmsms.
   В батче точную метку взять неоткуда, поэтому здесь показаны:
     - неверный формат метки -> ERROR (валидация),
     - откат к будущей метке  -> OK (берётся последний снимок).
   Содержательный откат удобнее показать интерактивно (см.
   examples/defense_guide.md).                                   */
REVERT students 2026.05.30;                          -- ERROR: неверный формат метки
REVERT students 2099.12.31-23:59:59.999;             -- OK: восстановлен последний снимок

/* ----- 16. DROP TABLE (на отдельной временной таблице) -------- */
//   students оставляем нетронутой, чтобы после прогона можно было
//   показать файл индекса data/demo/students/index_id.bpt и историю
CREATE TABLE tmp (id int INDEXED);
INSERT INTO tmp (id) VALUE (1);
DROP TABLE tmp;             -- удаление таблицы

/* ----- 17. Завершение ---------------------------------------- */
SELECT * FROM students;     -- финальное состояние таблицы students
EXIT;                       -- завершение сеанса (в батче — конец файла)
