CREATE TABLE doctors (
  ID SERIAL PRIMARY KEY,
  doctorID VARCHAR(255) NOT NULL,
  firstname VARCHAR(255) NOT NULL,
  lastname VARCHAR(255) NOT NULL,
  speciality VARCHAR(255) NOT NULL,
  sabbrev VARCHAR(255) NOT NULL,
  gender VARCHAR(255) NOT NULL,
  office VARCHAR(255) NOT NULL,
  email VARCHAR(255) NOT NULL,
  phone VARCHAR(255) NOT NULL,
  photo VARCHAR(255)
);

INSERT INTO doctors (doctorID,firstname,lastname,speciality,sabbrev,gender,office,email,phone)
VALUES  ('444432','Olubukola','Olu','Primary care,Internist','OO','Female','Room1','drolu@mail.com','444-234-333');

