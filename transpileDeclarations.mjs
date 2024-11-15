import { Project, ScriptKind, SyntaxKind } from 'ts-morph';
import { readFile, writeFile } from 'fs/promises';
import { argv } from 'process';
import sqlite3 from 'sqlite3';
const { Database, OPEN_READONLY, OPEN_FULLMUTEX } = sqlite3.verbose();
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

const project = new Project();

for (const file of argv) {
  const srcFile = project.addSourceFileAtPath(file);
  const promises = [];
  document(srcFile, promises);
  await Promise.all(promises);
}
await project.save();
db.close();

/**
 * add a docstring to node and recursively to its children.
 * @param {import('ts-morph').Node} node
 * @param {Array<Promise>} promiseStorage
 */
function document(node, promiseStorage) {
  promiseStorage.push(
    (async () => {
      if (node.isKind(SyntaxKind.MethodSignature)) {
        const parentName = (node.getParentWhileKind(SyntaxKind.TypeLiteral) ?? node).getParent().getName();
        let result = await all(
          `SELECT memberdef.rowid, memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription, memberdef.argsstring, (SELECT count(rowid) FROM memberdef_param WHERE memberdef_id = memberdef.rowid) as paramcount
  FROM memberdef
  JOIN member ON memberdef.rowid = member.memberdef_rowid
  JOIN compounddef ON member.scope_rowid = compounddef.rowid
  WHERE memberdef.kind = 'function' AND memberdef.name = $membername AND compounddef.name = $parentname AND paramcount = $paramcount`,
          { $membername: node.getName(), $parentname: parentName, $paramcount: node.getParameters().length }
        );
        console.debug(result);
        if (result.length > 0) {
          if (result.length > 1) throw new Error('ambiguous function declaration ' + node.getText() + ': possible overrides are ' + result.map(r => `${node.getName()}${r.argsstring}`).join(', '));
          else result = result[0];
          const parameters = await all(
            `SELECT param.type, param.declname
          FROM param
          JOIN memberdef_param ON param.rowid = memberdef_param.param_id
          JOIN memberdef ON memberdef_param.memberdef_id = memberdef.rowid
          WHERE memberdef.rowid = $rowid
          ORDER BY param.rowid ASC`,
            { $rowid: result.rowid }
          );
          console.debug(parameters.length, node.getParameters().length);
          node.getParameters().forEach((p, i) => {
            p.rename(parameters[i].declname ?? '_' + i);
          });
          if (result.briefdescription || result.detaileddescription || result.inbodydescription) {
            let doc = '';
            if (result.briefdescription) doc += xml2jsdoc(result.briefdescription);
            if (result.detaileddescription) doc += xml2jsdoc(result.detaileddescription);
            if (result.inbodydescription) doc += xml2jsdoc(result.inbodydescription);
            node.addJsDoc({
              description: doc.trim()
            });
          }
        }
      } else if (node.isKind(SyntaxKind.InterfaceDeclaration)) {
        const result = await get(
          `SELECT briefdescription, detaileddescription
  FROM compounddef
  WHERE kind = 'class' AND name = $membername`,
          { $membername: node.getName() }
        );
        if (result && (result.briefdescription || result.detaileddescription)) {
          let doc = '';
          if (result.briefdescription) doc += xml2jsdoc(result.briefdescription);
          if (result.detaileddescription) doc += xml2jsdoc(result.detaileddescription);
          node.addJsDoc({ description: doc.trim() });
        }
      } else if (node.isKind(SyntaxKind.PropertySignature)) {
        if (!node.getParentIfKind(SyntaxKind.TypeLiteral)) {
          // TODO: need support for type literals?
          const result = await get(
            `SELECT memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription
  FROM memberdef
  JOIN member ON memberdef.rowid = member.memberdef_rowid
  JOIN compounddef ON member.scope_rowid = compounddef.rowid
  WHERE memberdef.kind = 'function' AND memberdef.name = $membername AND compounddef.name = $parentname`,
            { $membername: node.getName(), $parentname: node.getParent().getName() }
          );
          // TODO: can this also be variables in c++, or macro definitions?
          if (result && (result.briefdescription || result.detaileddescription || result.inbodydescription)) {
            let doc = '';
            if (result.briefdescription) doc += xml2jsdoc(result.briefdescription);
            if (result.detaileddescription) doc += xml2jsdoc(result.detaileddescription);
            if (result.inbodydescription) doc += xml2jsdoc(result.inbodydescription);
            node.addJsDoc({ description: doc.trim() });
          }
        }
      }
    })()
  );
  node.forEachChild(n => document(n, promiseStorage));
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
