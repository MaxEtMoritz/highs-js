const stdout_lines = [];
const stderr_lines = [];

Module["print"] = (s) => {
  if (Module["logToConsole"]) console.log(s);
  stdout_lines.push(s);
};
Module["printErr"] = (s) => {
  if (Module["logToConsole"]) console.error(s);
  stderr_lines.push(s);
};
