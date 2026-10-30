Smart Student Performance & Result Management System  (MCA edition)

FILES (keep server.c, index.html, style.css, app.js in ONE folder)
  server.c        C backend: HTTP server + REST API, saves to students.dat
  index.html      HTML       - page structure
  style.css       CSS        - MCA theme, background picture, hover effects
  app.js          JavaScript - page behaviour + visual effects
  student_db.sql  Oracle PL/SQL - table, package, view (separate database version)
  run.bat         Windows: builds server.exe and starts it
  students.dat    created automatically on the first save (96-byte records)

SUBJECTS (7, each out of 100, total out of 700)
  1 Artificial Intelligence   2 Computer Network   3 Computer Aptitude
  4 Python   5 Web Programming   6 ADBMS   7 PL/SQL

NOTE: the old 5-subject students.dat (from pro.exe) is not compatible. If found, the
server renames it to students.dat.old and starts a new empty list.

QUICK START (Windows)
  1. Install MinGW / use Dev-C++'s gcc so "gcc" works in Command Prompt
  2. Double-click run.bat   (or: gcc server.c -o server.exe -lws2_32  then  server.exe)
  3. Open http://localhost:8080
  Another port: server.exe 9000

PL/SQL
  Run student_db.sql in SQL*Plus / SQL Developer: @student_db.sql
  It is NOT connected to server.c (the C server uses students.dat).

REST API
  GET /api/students | GET /api/students/101 | POST /api/students
  PUT /api/students/101 | DELETE /api/students/101 | GET /api/topper
