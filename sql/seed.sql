-- Development seed data for the OJ project.
-- Run after schema.sql, for example:
--   mysql -u oj_user -p oj < sql/seed.sql
--
-- The seeded login password for both accounts is "password".
-- password_hash values use the planned format:
--   pbkdf2_sha256$iterations$salt$hex_digest

SET NAMES utf8mb4;

START TRANSACTION;

INSERT INTO users (username, password_hash)
VALUES
  (
    'user1',
    'pbkdf2_sha256$120000$oj-user1-v1$559a3423ecafda16f42a4b30389959d58e637b750cbb65eb77edb31333bdf2c4'
  )
ON DUPLICATE KEY UPDATE
  password_hash = VALUES(password_hash);

INSERT INTO admins (username, password_hash)
VALUES
  (
    'admin',
    'pbkdf2_sha256$120000$oj-admin-v1$0202a5c043297dd8cf8f64563a05224a6956701381d6ff27f1ebfd5746613b78'
  )
ON DUPLICATE KEY UPDATE
  password_hash = VALUES(password_hash);

INSERT INTO problems (
  id,
  title,
  difficulty,
  description,
  input_format,
  output_format,
  sample_input,
  sample_output,
  time_limit_ms,
  memory_limit_kb,
  compare_mode
)
VALUES
  (
    1,
    'A+B Problem',
    'easy',
    'Read two integers a and b and output their sum.',
    'Two integers a and b separated by spaces.',
    'Print one integer: a + b.',
    '1 2\n',
    '3\n',
    1000,
    131072,
    'strict'
  ),
  (
    2,
    'Average Score',
    'easy',
    'Read n scores and print their average rounded to one decimal place.',
    'The first line contains n. The second line contains n integer scores.',
    'Print the average score with one digit after the decimal point.',
    '3\n1 2 4\n',
    '2.3\n',
    1000,
    131072,
    'float_1'
  )
ON DUPLICATE KEY UPDATE
  title = VALUES(title),
  difficulty = VALUES(difficulty),
  description = VALUES(description),
  input_format = VALUES(input_format),
  output_format = VALUES(output_format),
  sample_input = VALUES(sample_input),
  sample_output = VALUES(sample_output),
  time_limit_ms = VALUES(time_limit_ms),
  memory_limit_kb = VALUES(memory_limit_kb),
  compare_mode = VALUES(compare_mode);

INSERT INTO testcases (id, problem_id, `input`, expected_output, is_sample)
VALUES
  (1, 1, '1 2\n', '3\n', TRUE),
  (2, 1, '10 20\n', '30\n', FALSE),
  (3, 1, '-5 8\n', '3\n', FALSE),
  (4, 2, '3\n1 2 4\n', '2.3\n', TRUE),
  (5, 2, '4\n80 90 100 70\n', '85.0\n', FALSE),
  (6, 2, '1\n5\n', '5.0\n', FALSE)
ON DUPLICATE KEY UPDATE
  problem_id = VALUES(problem_id),
  `input` = VALUES(`input`),
  expected_output = VALUES(expected_output),
  is_sample = VALUES(is_sample);

COMMIT;
