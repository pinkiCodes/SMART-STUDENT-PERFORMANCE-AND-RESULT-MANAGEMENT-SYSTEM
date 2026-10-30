-- =====================================================================
-- Smart Student Performance & Result Management System
-- Oracle SQL + PL/SQL  (database version of the same logic)
--
-- Run in SQL*Plus / SQL Developer / Live SQL:   @student_db.sql
-- Needs Oracle 11g or newer (uses virtual columns).
-- =====================================================================

SET SERVEROUTPUT ON;

-- ---------------------------------------------------------------------
-- 1. TABLE  (one row per student, 7 subjects, each 0-100)
--    total and percentage are calculated by the database itself.
-- ---------------------------------------------------------------------
CREATE TABLE students (
    roll_no                NUMBER(10)    PRIMARY KEY,
    name                   VARCHAR2(49)  NOT NULL,
    artificial_intelligence NUMBER(5,2)  NOT NULL CHECK (artificial_intelligence BETWEEN 0 AND 100),
    computer_network       NUMBER(5,2)   NOT NULL CHECK (computer_network       BETWEEN 0 AND 100),
    computer_aptitude      NUMBER(5,2)   NOT NULL CHECK (computer_aptitude      BETWEEN 0 AND 100),
    python                 NUMBER(5,2)   NOT NULL CHECK (python                 BETWEEN 0 AND 100),
    web_programming        NUMBER(5,2)   NOT NULL CHECK (web_programming        BETWEEN 0 AND 100),
    adbms                  NUMBER(5,2)   NOT NULL CHECK (adbms                  BETWEEN 0 AND 100),
    plsql                  NUMBER(5,2)   NOT NULL CHECK (plsql                  BETWEEN 0 AND 100),
    total_marks            NUMBER(6,2)   GENERATED ALWAYS AS
                           (artificial_intelligence + computer_network + computer_aptitude +
                            python + web_programming + adbms + plsql) VIRTUAL,
    percentage             NUMBER(5,2)   GENERATED ALWAYS AS
                           ((artificial_intelligence + computer_network + computer_aptitude +
                             python + web_programming + adbms + plsql) / 7) VIRTUAL
);

-- ---------------------------------------------------------------------
-- 2. PACKAGE SPECIFICATION  (the list of things you can call)
-- ---------------------------------------------------------------------
CREATE OR REPLACE PACKAGE student_pkg AS
    FUNCTION grade_for (p_percentage NUMBER) RETURN VARCHAR2 DETERMINISTIC;
    FUNCTION status_for(p_percentage NUMBER) RETURN VARCHAR2 DETERMINISTIC;

    PROCEDURE add_student (p_roll NUMBER, p_name VARCHAR2,
                           p_ai NUMBER, p_cn NUMBER, p_apt NUMBER, p_py NUMBER,
                           p_web NUMBER, p_adbms NUMBER, p_plsql NUMBER);

    PROCEDURE update_student (p_roll NUMBER, p_name VARCHAR2,
                              p_ai NUMBER, p_cn NUMBER, p_apt NUMBER, p_py NUMBER,
                              p_web NUMBER, p_adbms NUMBER, p_plsql NUMBER);

    PROCEDURE delete_student (p_roll NUMBER);

    PROCEDURE print_result   (p_roll NUMBER);   -- result card (DBMS_OUTPUT)
    PROCEDURE find_topper;                      -- highest total (ties included)

    FUNCTION  ranked_students RETURN SYS_REFCURSOR;  -- sorted by total, highest first
END student_pkg;
/

-- ---------------------------------------------------------------------
-- 3. PACKAGE BODY  (the actual code)
-- ---------------------------------------------------------------------
CREATE OR REPLACE PACKAGE BODY student_pkg AS

    -- Same grade bands as the dashboard and the C server
    FUNCTION grade_for(p_percentage NUMBER) RETURN VARCHAR2 DETERMINISTIC IS
    BEGIN
        RETURN CASE
                   WHEN p_percentage >= 90 THEN 'A+'
                   WHEN p_percentage >= 80 THEN 'A'
                   WHEN p_percentage >= 70 THEN 'B'
                   WHEN p_percentage >= 60 THEN 'C'
                   WHEN p_percentage >= 50 THEN 'D'
                   WHEN p_percentage >= 40 THEN 'E'
                   ELSE 'F'
               END;
    END grade_for;

    FUNCTION status_for(p_percentage NUMBER) RETURN VARCHAR2 DETERMINISTIC IS
    BEGIN
        RETURN CASE WHEN p_percentage >= 40 THEN 'PASS' ELSE 'FAIL' END;
    END status_for;

    -- Check name and marks before saving
    PROCEDURE validate_input(p_name VARCHAR2, p_ai NUMBER, p_cn NUMBER, p_apt NUMBER,
                             p_py NUMBER, p_web NUMBER, p_adbms NUMBER, p_plsql NUMBER) IS
    BEGIN
        IF p_name IS NULL OR TRIM(p_name) IS NULL THEN
            RAISE_APPLICATION_ERROR(-20001, 'Enter the student''s name.');
        END IF;
        IF p_ai    NOT BETWEEN 0 AND 100 OR p_cn  NOT BETWEEN 0 AND 100 OR
           p_apt   NOT BETWEEN 0 AND 100 OR p_py  NOT BETWEEN 0 AND 100 OR
           p_web   NOT BETWEEN 0 AND 100 OR p_adbms NOT BETWEEN 0 AND 100 OR
           p_plsql NOT BETWEEN 0 AND 100 THEN
            RAISE_APPLICATION_ERROR(-20002, 'Each subject mark must be between 0 and 100.');
        END IF;
    END validate_input;

    -- ADD
    PROCEDURE add_student (p_roll NUMBER, p_name VARCHAR2,
                           p_ai NUMBER, p_cn NUMBER, p_apt NUMBER, p_py NUMBER,
                           p_web NUMBER, p_adbms NUMBER, p_plsql NUMBER) IS
    BEGIN
        IF p_roll IS NULL OR p_roll < 1 OR p_roll <> TRUNC(p_roll) THEN
            RAISE_APPLICATION_ERROR(-20003, 'Enter a valid positive roll number.');
        END IF;
        validate_input(p_name, p_ai, p_cn, p_apt, p_py, p_web, p_adbms, p_plsql);

        INSERT INTO students (roll_no, name, artificial_intelligence, computer_network,
                              computer_aptitude, python, web_programming, adbms, plsql)
        VALUES (p_roll, TRIM(p_name), p_ai, p_cn, p_apt, p_py, p_web, p_adbms, p_plsql);
        COMMIT;
        DBMS_OUTPUT.PUT_LINE('Student added successfully.');
    EXCEPTION
        WHEN DUP_VAL_ON_INDEX THEN
            RAISE_APPLICATION_ERROR(-20004, 'A student with this roll number already exists.');
    END add_student;

    -- UPDATE
    PROCEDURE update_student (p_roll NUMBER, p_name VARCHAR2,
                              p_ai NUMBER, p_cn NUMBER, p_apt NUMBER, p_py NUMBER,
                              p_web NUMBER, p_adbms NUMBER, p_plsql NUMBER) IS
    BEGIN
        validate_input(p_name, p_ai, p_cn, p_apt, p_py, p_web, p_adbms, p_plsql);

        UPDATE students
           SET name = TRIM(p_name), artificial_intelligence = p_ai, computer_network = p_cn,
               computer_aptitude = p_apt, python = p_py, web_programming = p_web,
               adbms = p_adbms, plsql = p_plsql
         WHERE roll_no = p_roll;

        IF SQL%ROWCOUNT = 0 THEN
            RAISE_APPLICATION_ERROR(-20005, 'Student not found.');
        END IF;
        COMMIT;
        DBMS_OUTPUT.PUT_LINE('Record updated.');
    END update_student;

    -- DELETE
    PROCEDURE delete_student (p_roll NUMBER) IS
    BEGIN
        DELETE FROM students WHERE roll_no = p_roll;
        IF SQL%ROWCOUNT = 0 THEN
            RAISE_APPLICATION_ERROR(-20005, 'Student not found.');
        END IF;
        COMMIT;
        DBMS_OUTPUT.PUT_LINE('Record deleted.');
    END delete_student;

    -- RESULT CARD
    PROCEDURE print_result (p_roll NUMBER) IS
        s students%ROWTYPE;
    BEGIN
        SELECT * INTO s FROM students WHERE roll_no = p_roll;

        DBMS_OUTPUT.PUT_LINE('==================== RESULT CARD ====================');
        DBMS_OUTPUT.PUT_LINE('Roll No : ' || s.roll_no);
        DBMS_OUTPUT.PUT_LINE('Name    : ' || s.name);
        DBMS_OUTPUT.PUT_LINE('-----------------------------------------------------');
        DBMS_OUTPUT.PUT_LINE(RPAD('Artificial Intelligence', 24) || LPAD(TO_CHAR(s.artificial_intelligence, 'FM990.00'), 10));
        DBMS_OUTPUT.PUT_LINE(RPAD('Computer Network', 24) || LPAD(TO_CHAR(s.computer_network, 'FM990.00'), 10));
        DBMS_OUTPUT.PUT_LINE(RPAD('Computer Aptitude', 24) || LPAD(TO_CHAR(s.computer_aptitude, 'FM990.00'), 10));
        DBMS_OUTPUT.PUT_LINE(RPAD('Python', 24) || LPAD(TO_CHAR(s.python, 'FM990.00'), 10));
        DBMS_OUTPUT.PUT_LINE(RPAD('Web Programming', 24) || LPAD(TO_CHAR(s.web_programming, 'FM990.00'), 10));
        DBMS_OUTPUT.PUT_LINE(RPAD('ADBMS', 24) || LPAD(TO_CHAR(s.adbms, 'FM990.00'), 10));
        DBMS_OUTPUT.PUT_LINE(RPAD('PL/SQL', 24) || LPAD(TO_CHAR(s.plsql, 'FM990.00'), 10));
        DBMS_OUTPUT.PUT_LINE('-----------------------------------------------------');
        DBMS_OUTPUT.PUT_LINE('Total      : ' || s.total_marks || ' / 700');
        DBMS_OUTPUT.PUT_LINE('Percentage : ' || s.percentage || '%');
        DBMS_OUTPUT.PUT_LINE('Grade      : ' || grade_for(s.percentage));
        DBMS_OUTPUT.PUT_LINE('Status     : ' || status_for(s.percentage));
        DBMS_OUTPUT.PUT_LINE('=====================================================');
    EXCEPTION
        WHEN NO_DATA_FOUND THEN
            RAISE_APPLICATION_ERROR(-20005, 'Student not found.');
    END print_result;

    -- TOPPER (shows every student tied for the highest total)
    PROCEDURE find_topper IS
        found BOOLEAN := FALSE;
    BEGIN
        FOR r IN (SELECT roll_no, name, total_marks, percentage
                    FROM students
                   WHERE total_marks = (SELECT MAX(total_marks) FROM students)
                   ORDER BY roll_no) LOOP
            found := TRUE;
            DBMS_OUTPUT.PUT_LINE('*** TOPPER ***  Roll ' || r.roll_no || '  ' || r.name ||
                                 '  ' || r.total_marks || '/700  (' || r.percentage || '%)');
        END LOOP;
        IF NOT found THEN
            DBMS_OUTPUT.PUT_LINE('No records found.');
        END IF;
    END find_topper;

    -- RANKING (dashboard "Topper & Rankings" page)
    FUNCTION ranked_students RETURN SYS_REFCURSOR IS
        c SYS_REFCURSOR;
    BEGIN
        OPEN c FOR
            SELECT RANK() OVER (ORDER BY total_marks DESC) AS rank_no,
                   roll_no, name, total_marks, percentage,
                   grade_for(percentage)  AS grade,
                   status_for(percentage) AS status
              FROM students
             ORDER BY total_marks DESC, roll_no;
        RETURN c;
    END ranked_students;

END student_pkg;
/

-- ---------------------------------------------------------------------
-- 4. VIEW  (ready-made list with grade and status, used by the dashboard)
-- ---------------------------------------------------------------------
CREATE OR REPLACE VIEW student_results AS
SELECT roll_no, name, artificial_intelligence, computer_network, computer_aptitude,
       python, web_programming, adbms, plsql,
       total_marks, percentage,
       student_pkg.grade_for(percentage)  AS grade,
       student_pkg.status_for(percentage) AS status
  FROM students;

-- ---------------------------------------------------------------------
-- 5. DEMO  (try it:  run these lines after the script above)
-- ---------------------------------------------------------------------
/*
BEGIN
    student_pkg.add_student(101, 'Asha Singh', 90, 80, 70, 60, 50, 85, 75);
    student_pkg.add_student(102, 'Ravi Kumar', 95, 99, 98, 97, 100, 96, 94);
    student_pkg.print_result(101);
    student_pkg.find_topper;
END;
/

SELECT * FROM student_results ORDER BY total_marks DESC;
*/
