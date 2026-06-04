-- MySQL 8 schema for the OJ project.
-- Run this after creating and selecting the target database, for example:
--   mysql -u oj_user -p oj < sql/schema.sql

SET NAMES utf8mb4;

CREATE TABLE IF NOT EXISTS users (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  username VARCHAR(64) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uq_users_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS admins (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  username VARCHAR(64) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uq_admins_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS problems (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  title VARCHAR(200) NOT NULL,
  difficulty VARCHAR(16) NOT NULL DEFAULT 'easy',
  description TEXT NOT NULL,
  input_format TEXT NOT NULL,
  output_format TEXT NOT NULL,
  sample_input TEXT NOT NULL,
  sample_output TEXT NOT NULL,
  time_limit_ms INT UNSIGNED NOT NULL DEFAULT 1000,
  memory_limit_kb INT UNSIGNED NOT NULL DEFAULT 131072,
  compare_mode VARCHAR(16) NOT NULL DEFAULT 'strict',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_problems_difficulty (difficulty),
  CONSTRAINT chk_problems_difficulty
    CHECK (difficulty IN ('easy', 'medium', 'hard')),
  CONSTRAINT chk_problems_compare_mode
    CHECK (compare_mode IN ('strict', 'float_1')),
  CONSTRAINT chk_problems_time_limit
    CHECK (time_limit_ms BETWEEN 1 AND 60000),
  CONSTRAINT chk_problems_memory_limit
    CHECK (memory_limit_kb BETWEEN 1024 AND 1048576)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS testcases (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  problem_id BIGINT UNSIGNED NOT NULL,
  `input` TEXT NOT NULL,
  expected_output TEXT NOT NULL,
  is_sample BOOLEAN NOT NULL DEFAULT FALSE,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_testcases_problem_id (problem_id),
  KEY idx_testcases_problem_sample (problem_id, is_sample),
  CONSTRAINT fk_testcases_problem
    FOREIGN KEY (problem_id) REFERENCES problems (id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
