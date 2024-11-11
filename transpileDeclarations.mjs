import ts from 'typescript';
import { readFile, writeFile } from 'fs/promises';
import { argv } from 'process';
import sqlite3 from 'sqlite3';
const { Database, OPEN_READONLY, OPEN_FULLMUTEX } = sqlite3.verbose();
import { promisify } from 'util';
import { resolve } from 'path';
argv.splice(0, 2);
console.debug(resolve('sqlite3', 'doxygen_sqlite3.db'), argv);
const db = new Database(resolve('sqlite3', 'doxygen_sqlite3.db'), OPEN_READONLY | OPEN_FULLMUTEX, e => console.error(e));
/**
 * Fetch a single row from DB
 * @param {string} query Query to execute
 * @param {{[name:string]:any}|undefined} params optional parameter object (reference in query by name)
 * @returns {Promise<{[colanme:string]:any}>} result row
 */
const get = (query, params) =>
  new Promise((y, n) => {
    const cb = function (err, res) {
      if (err) n(err);
      else y(res);
    };
    if (params) db.get(query, params, cb);
    else db.get(query, cb);
  });
/**
 * Fetch multiple rows from DB
 * @param {string} query Query to execute
 * @param {{[name:string]:any}|undefined} params optional parameter object (reference in query by name)
 * @returns {Promise<{[colanme:string]:any}[]>} array of result rows
 */
const all = (query, params) =>
  new Promise((y, n) => {
    const cb = function (err, res) {
      if (err) n(err);
      else y(res);
    };
    if (params) db.all(query, params, cb);
    else db.all(query, cb);
  });
// db.on('error', query => console.error('db error for ' + query));
// db.on('trace', query => console.debug('running ' + query));
// db.on('profile', query => console.debug('done running ' + query));
// db.on('open', () => console.debug('db opened'));
// db.on('close', () => console.debug('db closed'));
// db.on('change', () => console.debug('db changed'));

for (const file of argv) {
  const srcFile = ts.createSourceFile(file, await readFile(file, { encoding: 'utf8' }), ts.ScriptTarget.Latest, true, ts.ScriptKind.TS);
  const promises = [];
  document(srcFile, promises);
  await Promise.all(promises);
  const printer = ts.createPrinter();
  await writeFile(file, printer.printFile(srcFile));
}
db.close();

/**
 * add a docstring to node and recursively to its children.
 * @param {import('typescript').Node} node
 * @param {Array<Promise>} promiseStorage
 */
function document(node, promiseStorage) {
  promiseStorage.push(
    (async () => {
      if (ts.isMethodSignature(node)) {
        const result = await get(
          `SELECT memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription
  FROM memberdef
  JOIN member ON memberdef.rowid = member.memberdef_rowid
  JOIN compounddef ON member.scope_rowid = compounddef.rowid
  WHERE memberdef.kind = 'function' AND memberdef.name = $membername AND compounddef.name = $parentname`,
          { $membername: node.name.text, $parentname: node.parent.name.text }
        );
        if (result && (result.briefdescription || result.detaileddescription || result.inbodydescription)) {
          let doc = '*\n';
          if (result.briefdescription) doc += xml2jsdoc(result.briefdescription);
          if (result.detaileddescription) doc += xml2jsdoc(result.detaileddescription);
          if (result.inbodydescription) doc += xml2jsdoc(result.inbodydescription);
          ts.addSyntheticLeadingComment(node, ts.SyntaxKind.MultiLineCommentTrivia, doc.trim() + '\n', true);
        }
      } else if (ts.isInterfaceDeclaration(node)) {
        const result = await get(
          `SELECT briefdescription, detaileddescription
  FROM compounddef
  WHERE kind = 'class' AND name = $membername`,
          { $membername: node.name.text }
        );
        if (result && (result.briefdescription || result.detaileddescription)) {
          let doc = '*\n';
          if (result.briefdescription) doc += xml2jsdoc(result.briefdescription);
          if (result.detaileddescription) doc += xml2jsdoc(result.detaileddescription);
          ts.addSyntheticLeadingComment(node, ts.SyntaxKind.MultiLineCommentTrivia, doc.trim() + '\n', true);
        }
      } else if (ts.isPropertySignature(node)) {
        if (!ts.isTypeLiteralNode(node.parent)) {
          // TODO: need support for type literals?
          const result = await get(
            `SELECT memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription
  FROM memberdef
  JOIN member ON memberdef.rowid = member.memberdef_rowid
  JOIN compounddef ON member.scope_rowid = compounddef.rowid
  WHERE memberdef.kind = 'function' AND memberdef.name = $membername AND compounddef.name = $parentname`,
            { $membername: node.name.text, $parentname: node.parent.name.text }
          );
          // TODO: can this also be variables in c++, or macro definitions?
          if (result && (result.briefdescription || result.detaileddescription || result.inbodydescription)) {
            let doc = '*\n';
            if (result.briefdescription) doc += xml2jsdoc(result.briefdescription);
            if (result.detaileddescription) doc += xml2jsdoc(result.detaileddescription);
            if (result.inbodydescription) doc += xml2jsdoc(result.inbodydescription);
            ts.addSyntheticLeadingComment(node, ts.SyntaxKind.MultiLineCommentTrivia, doc.trim() + '\n', true);
          }
        }
      }
    })()
  );
  ts.forEachChild(node, n => document(n, promiseStorage));
}

/**
 *
 * @param {string} xml
 */
function xml2jsdoc(xml) {
  // https://www.doxygen.nl/manual/xmlcmds.html

  // <para>...</para> --> ... \n\n
  const para = /<para>(.*?)<\/para>/gs;
  xml = xml.replace(para, '$1\n\n');

  // <ref refid="..." kindref="...">...</ref> --> {@link ...}
  // TODO: lookup refid in DB to check parent name

  // TODO: maybe more

  return xml;
}
