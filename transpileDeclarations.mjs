import {
  ConstructSignatureDeclaration,
  FunctionDeclaration,
  GetAccessorDeclaration,
  InterfaceDeclaration,
  MethodSignature,
  Project,
  PropertySignature,
  ScriptTarget,
  SetAccessorDeclaration,
  SourceFile,
  SyntaxKind,
  TypeAliasDeclaration,
  TypeFormatFlags
} from 'ts-morph';
import { argv } from 'node:process';
import sqlite3 from 'sqlite3';
const { Database, OPEN_READONLY, OPEN_FULLMUTEX } = sqlite3.verbose();
import { resolve } from 'node:path';
import { parseArgs } from 'node:util';
import { readFile } from 'node:fs/promises';

argv.splice(0, 2);
const { values: args, positionals: files } = parseArgs({
  allowPositionals: true,
  allowNegative: true,
  options: {
    'doxygen-db': {
      type: 'string',
      default: 'sqlite3/doxygen_sqlite3.db',
      short: 'd'
    },
    'mapping-file': {
      type: 'string',
      short: 'm'
    },
    'warn-unmapped': {
      type: 'boolean',
      short: 'w',
      default: true
    }
  },
  args: argv
});
/**@type {Map<MappingInfo, MappingInfo>} */
let mapping = new Map();
if (args['mapping-file']) {
  const mappingText = await readFile(args['mapping-file'], { encoding: 'utf8' });
  mapping = parseMapping(mappingText);
  console.debug(mapping);
}
const db = new Database(resolve(args['doxygen-db']), OPEN_READONLY, e => (e ? console.error(e) : void null));
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
for (const file of files) {
  const srcFile = project.addSourceFileAtPath(file);
  await documentV2(srcFile);
}
await project.save();
db.close();

/**
 *
 * @param {SourceFile} file
 */
async function documentV2(file) {
  // get all interfaces
  let classes = file.getInterfaces();
  let embindModule = classes.splice(
    classes.findIndex(i => i.getName() == 'EmbindModule'),
    1
  )[0];
  let moduleMembers = embindModule.getMembers();

  let classHandle = classes.splice(
    classes.findIndex(i => i.getName() == 'ClassHandle'),
    1
  )[0];

  for (const method of classHandle.getMethods()) {
    method.getJsDocs().forEach(doc => doc.remove());
    switch (method.getName()) {
      case 'isAliasOf':
        method.addJsDoc({
          // TODO: is this documentation string correct?
          description: 'Determines if this object references the same underlying C++ object as another JS object',
          tags: [{ tagName: 'param', text: 'other the other object to check' }]
        });
        break;
      case 'delete':
        method.addJsDoc({
          description:
            'deletes the underlying C++ object synchronously and frees the associated memory.\n\nUsing any properties or methods except {@link ClassHandle.isDeleted | isDeleted()} after deletion leads to exceptions'
        });
        break;
      case 'deleteLater':
        method.addJsDoc({
          description:
            'schedules the underlying C++ object for deletion and returns immediately. The object will be deleted in the background after some time.\n\n\
Using any properties or methods except {@link ClassHandle.isDeleted | isDeleted()} after scheduling the object for deletion leads to exceptions'
        });
        break;
      case 'isDeleted':
        method.addJsDoc({ description: 'checks if the underlying C++ object is already deleted (returns true if the object was scheduled for deletion but does still exist).' });
        break;
      case 'clone':
        method.addJsDoc({ description: 'returns a new JS object that references the same underlying C++ object' });
        break;
      default:
        console.warn('Unexpected method ' + method.getName() + ' on interface ClassHandle.');
    }
  }

  // don't know what it contains, but for Highs it's been always empty
  let wasmModule = classes.splice(
    classes.findIndex(i => i.getName() == 'WasmModule'),
    1
  )[0];

  for (const cls of classes) {
    if (cls.getExtends().some(e => e.getChildrenOfKind(SyntaxKind.Identifier)[0].getText() == 'ClassHandle')) {
      // this is a c++ class, struct or vector
      let moduleProp = embindModule.getProperty(cls.getName());
      moduleMembers.splice(moduleMembers.indexOf(moduleProp), 1);
      let moduleBody = moduleProp.getChildrenOfKind(SyntaxKind.TypeLiteral)[0];
      const mapping = findMapping(undefined, cls.getName(), null);
      let className = mapping ? mapping.name : cls.getName();
      let result = await all(
        `SELECT briefdescription, detaileddescription
FROM compounddef
WHERE (kind = 'class' OR kind = 'struct') AND name = $membername`,
        { $membername: className }
      );
      if (result.length > 0) {
        if (result.length > 1) throw new Error('ambiguous class ' + cls.getName() + '.');
        else result = result[0];
        if (result.briefdescription || result.detaileddescription) {
          cls.getJsDocs()
          .forEach(doc => doc.remove());
          moduleProp.getJsDocs()
          .forEach(doc => doc.remove());
          let doc = '';
          if (result.briefdescription) doc += await xml2jsdoc(result.briefdescription);
          if (result.detaileddescription) doc += await xml2jsdoc(result.detaileddescription);
          cls.addJsDoc({ description: doc.trim() });
          moduleProp.addJsDoc({ description: doc.trim() });
        }
        // find constructor(s)
        for (const constructor of moduleBody.getConstructSignatures()) {
          await documentFunction(constructor, moduleProp, className);
        }
        // find class methods
        for (const classmethod of moduleBody.getMethods()) {
          await documentFunction(classmethod, moduleProp, className);
        }
        // find class properties
        for (const property of moduleBody.getProperties().concat(moduleBody.getSetAccessors(), moduleBody.getGetAccessors())) {
          await documentProperty(property, moduleProp, className);
        }
        // find methods
        for (const method of cls.getMethods()) {
          await documentFunction(method, cls, className);
        }
        // find properties
        for (const property of cls.getProperties().concat(cls.getSetAccessors(), cls.getGetAccessors())) {
          await documentProperty(property, cls, className);
        }
      } else if (args['warn-unmapped']) console.warn(`the class / struct ${cls.getName()} was not found. specify a mapping in the mappings file (-m option)`);
    } else {
      if (cls.getName().endsWith('Value')) {
        // this is an enum wrapper
        let moduleProp = embindModule.getProperty(cls.getName().substring(0, cls.getName().lastIndexOf('Value')));
        moduleMembers.splice(moduleMembers.indexOf(moduleProp), 1);
        let typedef = file.getTypeAlias(moduleProp.getName());
        await documentEnum(cls, moduleProp, typedef);
      } else {
        // this is something different
        console.warn('unhandled interface ' + cls.getName());
      }
    }
  }

  // TODO: property cannot cope with null parent
  // for (const constant of moduleMembers) {
  //   await documentProperty(constant, undefined, undefined);
  // }
}

/**
 *
 * @param {MethodSignature | ConstructSignatureDeclaration} node
 * @param {InterfaceDeclaration | PropertySignature} parent
 * @param {string} className
 */
async function documentFunction(node, parent, className) {
  let result;
  let parentName = className;
  let methodName = node.isKind(SyntaxKind.MethodSignature) ? node.getName() : className;
  const mapping = findMapping(parent.getName(), node.isKind(SyntaxKind.MethodSignature) ? node.getName() : parent.getName(), ...node.getParameters().map(p => p.getType().getText(node)));
  if (mapping) {
    parentName = mapping.member;
    methodName = mapping.name;
    // TODO: parameter type filter
    result = await all(
      `SELECT memberdef.rowid, memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription, json_group_array(param.declname ORDER BY (SELECT instr(memberdef.argsstring, param.declname))) filter (
    where
      param.declname is not null
  ) as paramNames, json_group_array(replace(replace(param.type,'const ',''),' ','') ORDER BY (SELECT instr(memberdef.argsstring, param.declname))) filter (
    where
      param.type is not null
  ) as paramTypes, compounddef.name as parent
  FROM memberdef
  JOIN member ON memberdef.rowid = member.memberdef_rowid
  JOIN compounddef ON member.scope_rowid = compounddef.rowid
  LEFT JOIN memberdef_param ON memberdef_param.memberdef_id = memberdef.rowid
  LEFT JOIN param on param.rowid = memberdef_param.param_id
  WHERE memberdef.kind = 'function' AND memberdef.name = $name
  GROUP BY memberdef.rowid
  HAVING paramTypes = $params`,
      { $params: JSON.stringify(mapping.args), $name: methodName }
    );
    if (parentName) result = result.filter(r => r.parent == parentName);
    if (result.length !== 1) console.debug(result.length);
  } else
    result = await all(
      `SELECT memberdef.rowid, memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription, memberdef.argsstring, json_group_array(param.declname ORDER BY (SELECT instr(memberdef.argsstring, param.declname))) filter (
    where
      param.declname is not null
  ) as paramNames, json_group_array(replace(replace(param.type,'const ',''),' ','') ORDER BY (SELECT instr(memberdef.argsstring, param.declname))) filter (
    where
      param.type is not null
  ) as paramTypes
  FROM memberdef
  JOIN member ON memberdef.rowid = member.memberdef_rowid
  JOIN compounddef ON member.scope_rowid = compounddef.rowid
  LEFT JOIN memberdef_param ON memberdef_param.memberdef_id = memberdef.rowid
  LEFT JOIN param on param.rowid = memberdef_param.param_id
  WHERE memberdef.kind = 'function' AND memberdef.name = $membername AND compounddef.name = $parentname
  GROUP BY memberdef.rowid
  HAVING count(param.rowid) = $paramcount`,
      { $membername: methodName, $parentname: parentName, $paramcount: node.getParameters().length }
    );
  if (parentName == 'std::vector') {
    if (methodName == 'push_back' && result.length == 2) {
      // there is push_back(value_type&) and push_back(value_type&&) but it doesn't matter wich doc to take
      result.pop();
    } else if (methodName == 'get') {
      // also in emscripten/bind.h emscripten::internal::VectorAccess<VectorType>::get, but has no doc text there
      node.getJsDocs().forEach(doc => doc.remove());
      let params = node.getParameters();
      params[0].rename('i');
      node.addJsDoc({
        description: 'returns the `i`th element of the vector if present'
      });
      return;
    } else if (methodName == 'set') {
      // also in emscripten/bind.h emscripten::internal::VectorAccess<VectorType>::set, but has no doc text there
      node.getJsDocs().forEach(doc => doc.remove());
      let params = node.getParameters();
      params[0].rename('i');
      params[1].rename('v');
      node.addJsDoc({
        description: 'sets the `i`th element of the vector to `v`, if possible',
        tags: [{ tagName: 'returns', text: '`true` if the vector could be modified. `false` otherwise' }]
      });
      return;
    } else if (methodName == className) {
      return;
    }
  }
  //console.debug(result);
  if (result.length > 0) {
    if (result.length > 1) throw new Error('ambiguous method declaration ' + node.getText() + ': possible overrides are ' + result.map(r => `${methodName}${r.argsstring}`).join(', '));
    else result = result[0];
    const parameters = JSON.parse(result.paramNames);
    if (!result.paramTypes) debugger;
    const paramTypes = JSON.parse(result.paramTypes);
    if (parameters.length == node.getParameters().length + 1 && paramTypes[0] == className + '&') parameters.shift();
    console.assert(parameters.length == node.getParameters().length, `number of method parameters differs on ${node.getText()}: expected ${node.getParameters().length}, got ${parameters.length}`);
    node.getParameters().forEach((p, i) => {
      p.rename(parameters[i] ?? '_' + i);
    });
    if (result.briefdescription || result.detaileddescription || result.inbodydescription) {
      node.getJsDocs().forEach(doc => doc.remove());
      let doc = '';
      if (result.briefdescription) doc += await xml2jsdoc(result.briefdescription);
      if (result.detaileddescription) doc += await xml2jsdoc(result.detaileddescription);
      if (result.inbodydescription) doc += await xml2jsdoc(result.inbodydescription);
      node.addJsDoc({
        description: doc.trim()
      });
    }
  } else if (args['warn-unmapped']) {
    if (node.isKind(SyntaxKind.ConstructSignature) && node.getParameters().length < 1) console.info(`the constructor ${node.getText()} was not found and skipped (does not have parameters).`);
    else console.warn(`the method ${node.getText()} was not found. specify a mapping in the mappings file (-m option)`);
  }
}

/**
 *
 * @param {PropertySignature | GetAccessorDeclaration | SetAccessorDeclaration} node
 * @param {InterfaceDeclaration | PropertySignature} parent
 * @param {string} className
 */
async function documentProperty(node, parent, className) {
  const mapping = findMapping(parent.getName(), node.getName(), null);
  let result,
    parentName = className,
    propertyName = node.getName();
  if (mapping) {
    parentName = mapping.member;
    propertyName = mapping.name;
  }
  result = await all(
    `SELECT memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription
FROM memberdef
JOIN member ON memberdef.rowid = member.memberdef_rowid
JOIN compounddef ON member.scope_rowid = compounddef.rowid
WHERE (memberdef.kind = 'function' OR memberdef.kind = 'variable') AND memberdef.name = $membername AND compounddef.name = $parentname`,
    { $membername: propertyName, $parentname: parentName }
  );
  if (result.length > 0) {
    if (result.length > 1) throw new Error('ambiguous property ' + node.getName() + '.');
    else result = result[0];
    if (result.briefdescription || result.detaileddescription || result.inbodydescription) {
      node.getJsDocs().forEach(doc => doc.remove());
      let doc = '';
      if (result.briefdescription) doc += await xml2jsdoc(result.briefdescription);
      if (result.detaileddescription) doc += await xml2jsdoc(result.detaileddescription);
      if (result.inbodydescription) doc += await xml2jsdoc(result.inbodydescription);
      node.addJsDoc({ description: doc.trim() });
    }
  } else if (args['warn-unmapped']) console.warn(`the property ${node.getName()} was not found. specify a mapping in the mappings file (-m option)`);
}

/**
 *
 * @param {InterfaceDeclaration} wrapper
 * @param {PropertySignature} moduleProp
 * @param {TypeAliasDeclaration} typedef
 */
async function documentEnum(wrapper, moduleProp, typedef) {
  const mapping = findMapping(undefined, moduleProp.getName(), null);
  let result,
    //parentName = undefined,
    enumName = moduleProp.getName();
  if (mapping) {
    //parentName = mapping.member;
    enumName = mapping.name;
  }
  result = await all(
    `SELECT memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription
FROM memberdef
--JOIN member ON memberdef.rowid = member.memberdef_rowid
--JOIN compounddef ON member.scope_rowid = compounddef.rowid
WHERE memberdef.kind = 'enumeration' AND memberdef.name = $membername --AND compounddef.name = $parentname`,
    { $membername: enumName /*, $parentname: parentName*/ }
  );
  if (result.length > 0) {
    if (result.length > 1) throw new Error('ambiguous enum ' + enumName + '.');
    else result = result[0];
    if (result.briefdescription || result.detaileddescription || result.inbodydescription) {
      moduleProp
        .getJsDocs()
        .concat(typedef.getJsDocs())
        .forEach(doc => doc.remove());
      let doc = '';
      if (result.briefdescription) doc += await xml2jsdoc(result.briefdescription);
      if (result.detaileddescription) doc += await xml2jsdoc(result.detaileddescription);
      if (result.inbodydescription) doc += await xml2jsdoc(result.inbodydescription);
      moduleProp.addJsDoc({ description: doc.trim() });
      typedef.addJsDoc({ description: doc.trim() });
    }
    // TODO: how to get Doxygen to include enum values?
    // for (const enumMember of moduleProp.getChildrenOfKind(SyntaxKind.TypeLiteral)[0].getProperties()) {
    //   await documentProperty(enumMember, moduleProp, enumName);
    // }
    wrapper.getJsDocs().forEach(doc => doc.remove());
    wrapper.addJsDoc({ description: 'Wrapper class for ' + moduleProp.getName() + ' enum.' });
  } else if (args['warn-unmapped']) console.warn(`the enum ${moduleProp.getName()} was not found. specify a mapping in the mappings file (-m option)`);
}

// /**
//  * add a docstring to node and recursively to its children.
//  * @param {import('ts-morph').Node} node
//  * @param {Array<Promise>} promiseStorage
//  */
// function document(node, promiseStorage) {
//   promiseStorage.push(
//     (async () => {
//       if (node.isKind(SyntaxKind.MethodSignature) || node.isKind(SyntaxKind.ConstructSignature)) {
//         let parentNode = (node.getParentWhileKind(SyntaxKind.TypeLiteral) ?? node).getParent();
//         let parentName = parentNode.getName();
//         let membername = node.isKind(SyntaxKind.MethodSignature) ? node.getName() : parentName;
//         const mapping = findMapping(parentName, membername, ...node.getParameters().map(p => p.getType().getText(node)));
//         let result;
//         if (mapping) {
//           parentName = mapping.member;
//           membername = mapping.name;
//           // TODO: parameter type filter
//           result = await all(
//             `SELECT memberdef.rowid, memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription, json_group_array(param.declname ORDER BY param.rowid) filter (
//     where
//       param.declname is not null
//   ) as paramNames, json_group_array(replace(replace(param.type,'const ',''),' ','') ORDER BY param.rowid) filter (
//     where
//       param.type is not null
//   ) as paramTypes, compounddef.name as parent
//   FROM memberdef
//   JOIN member ON memberdef.rowid = member.memberdef_rowid
//   JOIN compounddef ON member.scope_rowid = compounddef.rowid
//   LEFT JOIN memberdef_param ON memberdef_param.memberdef_id = memberdef.rowid
//   LEFT JOIN param on param.rowid = memberdef_param.param_id
//   WHERE memberdef.kind = 'function' AND memberdef.name = $name
//   GROUP BY memberdef.rowid
//   HAVING paramTypes = $params`,
//             { $params: JSON.stringify(mapping.args), $name: mapping.name }
//           );
//           if (mapping.member) result = result.filter(r => r.parent == mapping.member);
//           if (result.length !== 1) debugger;
//         } else
//           result = await all(
//             `SELECT memberdef.rowid, memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription, memberdef.argsstring, json_group_array(param.declname ORDER BY param.rowid) filter (
//     where
//       param.declname is not null
//   ) as paramNames, json_group_array(replace(replace(param.type,'const ',''),' ','') ORDER BY param.rowid) filter (
//     where
//       param.type is not null
//   ) as paramTypes
//   FROM memberdef
//   JOIN member ON memberdef.rowid = member.memberdef_rowid
//   JOIN compounddef ON member.scope_rowid = compounddef.rowid
//   LEFT JOIN memberdef_param ON memberdef_param.memberdef_id = memberdef.rowid
//   LEFT JOIN param on param.rowid = memberdef_param.param_id
//   WHERE memberdef.kind = 'function' AND memberdef.name = $membername AND compounddef.name = $parentname
//   GROUP BY memberdef.rowid
//   HAVING count(param.rowid) = $paramcount`,
//             { $membername: membername, $parentname: parentName, $paramcount: node.getParameters().length }
//           );
//         //console.debug(result);
//         if (result.length > 0) {
//           if (result.length > 1) throw new Error('ambiguous function declaration ' + node.getText() + ': possible overrides are ' + result.map(r => `${membername}${r.argsstring}`).join(', '));
//           else result = result[0];
//           const parameters = JSON.parse(result.paramNames);
//           if (!result.paramTypes) debugger;
//           const paramTypes = JSON.parse(result.paramTypes);
//           if (parameters.length == node.getParameters().length + 1 && paramTypes[0] == parentNode.getName() + '&') parameters.shift();
//           console.assert(
//             parameters.length == node.getParameters().length,
//             `number of function parameters differs on ${parentName}.${membername}: expected ${node.getParameters().length}, got ${parameters.length}`
//           );
//           node.getParameters().forEach((p, i) => {
//             p.rename(parameters[i] ?? '_' + i);
//           });
//           if (result.briefdescription || result.detaileddescription || result.inbodydescription) {
//             let doc = '';
//             if (result.briefdescription) doc += await xml2jsdoc(result.briefdescription);
//             if (result.detaileddescription) doc += await xml2jsdoc(result.detaileddescription);
//             if (result.inbodydescription) doc += await xml2jsdoc(result.inbodydescription);
//             node.addJsDoc({
//               description: doc.trim()
//             });
//           }
//         } else if (args['warn-unmapped'])
//           console.warn(
//             `the method ${parentName}.${membername}(${node
//               .getParameters()
//               .map(p => p.getText(node))
//               .join(', ')}) was not found. specify a mapping in the mappings file (-m option)`
//           );
//       } else if (node.isKind(SyntaxKind.InterfaceDeclaration)) {
//         if (
//           node
//             .getSourceFile()
//             .getInterface('EmbindModule')
//             .getChildrenOfKind(SyntaxKind.PropertySignature)
//             .some(c => c.getName() + 'Value' == node.getName())
//         ) {
//           // this is an enum wrapper
//           let name = node.getName();
//           node.addJsDoc({
//             description: 'Wrapper class for ' + name.substring(0, name.length - 'Value'.length) + ' enum.'
//           });
//         } else {
//           const mapping = findMapping(undefined, node.getName(), null);
//           let result = await all(
//             `SELECT briefdescription, detaileddescription
//   FROM compounddef
//   WHERE (kind = 'class' OR kind = 'struct') AND name = $membername`,
//             { $membername: mapping ? mapping.name : node.getName() }
//           );
//           if (result.length > 0) {
//             if (result.length > 1) throw new Error('ambiguous class ' + node.getName() + '.');
//             else result = result[0];
//             if (result.briefdescription || result.detaileddescription) {
//               let doc = '';
//               if (result.briefdescription) doc += await xml2jsdoc(result.briefdescription);
//               if (result.detaileddescription) doc += await xml2jsdoc(result.detaileddescription);
//               node.addJsDoc({ description: doc.trim() });
//             }
//           } else if (args['warn-unmapped']) console.warn(`the class ${node.getName()} was not found. specify a mapping in the mappings file (-m option)`);
//         }
//       } else if (node.isKind(SyntaxKind.PropertySignature)) {
//         if (!node.getParentIfKind(SyntaxKind.TypeLiteral)) {
//           // TODO: need support for type literals?
//           if (node.getParent().getName() == 'EmbindModule') {
//             // TODO: request enum/property (member) or class (compound)
//             if (node.getSourceFile().getInterface(node.getName())) {
//               // this is a class (compound)
//               const mapping = findMapping(undefined, node.getName(), null);
//               let result = await all(
//                 `SELECT briefdescription, detaileddescription
//   FROM compounddef
//   WHERE (kind = 'class' OR kind = 'struct') AND name = $membername`,
//                 { $membername: mapping ? mapping.name : node.getName() }
//               );
//               if (result.length > 0) {
//                 if (result.length > 1) throw new Error('ambiguous class ' + node.getName() + '.');
//                 else result = result[0];
//                 if (result.briefdescription || result.detaileddescription) {
//                   let doc = '';
//                   if (result.briefdescription) doc += await xml2jsdoc(result.briefdescription);
//                   if (result.detaileddescription) doc += await xml2jsdoc(result.detaileddescription);
//                   node.addJsDoc({ description: doc.trim() });
//                 }
//               } else if (args['warn-unmapped']) console.warn(`the class ${node.getName()} was not found. specify a mapping in the mappings file (-m option)`);
//             } else {
//               // this is a property, enum, etc. (member)
//               let result = await all(
//                 `SELECT memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription
//     FROM memberdef
//     --JOIN member ON memberdef.rowid = member.memberdef_rowid
//     --JOIN compounddef ON member.scope_rowid = compounddef.rowid
//     WHERE (memberdef.kind = 'function' OR memberdef.kind = 'variable' OR memberdef.kind = 'enumeration') AND memberdef.name = $membername`,
//                 { $membername: node.getName() }
//               );
//               if (result.length > 0) {
//                 if (result.length > 1) throw new Error('ambiguous proprty ' + node.getName() + '.');
//                 else result = result[0];
//                 if (result.briefdescription || result.detaileddescription || result.inbodydescription) {
//                   let doc = '';
//                   if (result.briefdescription) doc += await xml2jsdoc(result.briefdescription);
//                   if (result.detaileddescription) doc += await xml2jsdoc(result.detaileddescription);
//                   if (result.inbodydescription) doc += await xml2jsdoc(result.inbodydescription);
//                   node.addJsDoc({ description: doc.trim() });
//                 }
//               } else if (args['warn-unmapped']) console.warn(`the property ${node.getName()} was not found. specify a mapping in the mappings file (-m option)`);
//             }
//           } else {
//             let result = await all(
//               `SELECT memberdef.briefdescription, memberdef.detaileddescription, memberdef.inbodydescription
//   FROM memberdef
//   JOIN member ON memberdef.rowid = member.memberdef_rowid
//   JOIN compounddef ON member.scope_rowid = compounddef.rowid
//   WHERE (memberdef.kind = 'function' OR memberdef.kind = 'variable') AND memberdef.name = $membername AND compounddef.name = $parentname`,
//               { $membername: node.getName(), $parentname: node.getParent().getName() }
//             );
//             // TODO: can this also be variables in c++, or macro definitions?
//             if (result.length > 0) {
//               if (result.length > 1) throw new Error('ambiguous proprty ' + node.getName() + '.');
//               else result = result[0];
//               if (result.briefdescription || result.detaileddescription || result.inbodydescription) {
//                 let doc = '';
//                 if (result.briefdescription) doc += await xml2jsdoc(result.briefdescription);
//                 if (result.detaileddescription) doc += await xml2jsdoc(result.detaileddescription);
//                 if (result.inbodydescription) doc += await xml2jsdoc(result.inbodydescription);
//                 node.addJsDoc({ description: doc.trim() });
//               }
//             } else if (args['warn-unmapped']) console.warn(`the property ${node.getName()} was not found. specify a mapping in the mappings file (-m option)`);
//           }
//         }
//       }
//     })()
//   );
//   node.forEachChild(n => document(n, promiseStorage));
// }

/**
 *
 * @param {string} xml
 */
async function xml2jsdoc(xml) {
  // https://www.doxygen.nl/manual/xmlcmds.html

  // <para>...</para> --> ... \n\n
  const para = /<para>(.*?)<\/para>/gs;
  xml = xml.replace(para, '$1\n\n');

  // <ref refid="..." kindref="...">...</ref> --> {@link ...}
  // TODO: lookup refid in DB to check parent name
  const ref = /<ref refid="(.*?)" kindref="(.*?)">(.*?)<\/ref>/gs;
  for (const match of xml.matchAll(ref)) {
    if (match[2] === 'compound') {
      // class/interface etc. is referenced --> no need for parent name
      const result = await get(`SELECT name FROM def WHERE refid = $refid`, { $refid: match[1] });
      if (!result) continue;
      xml = xml.replace(match[0], `{@link ${result.name} | ${match[3]}}`);
    } else if (match[2] === 'member') {
      // method, property, etc. referenced. --> need for parent name
      const result = await get(
        `SELECT def.name, compounddef.name as parentName FROM def
        JOIN member ON member.memberdef_rowid = def.rowid
        JOIN compounddef ON compounddef.rowid = member.scope_rowid
        WHERE refid = $refid`,
        { $refid: match[1] }
      );
      if (!result) continue;
      xml = xml.replace(match[0], `{@link ${result.parentName}.${result.name} | ${match[3]}}`);
    } else {
      console.warn('unhandled ref type:', match[2]);
    }
  }

  // <linebreak/> --> <br>
  const linebreak = /<\s*linebreak\s*\/>/gs;
  xml = xml.replace(linebreak, '<br>');

  // remove <parameterlist ...>....</parameterlist>
  const parameterlist = /<parameterlist.*?>.*?<\/parameterlist>/gs;
  xml = xml.replace(parameterlist, '');

  // TODO: maybe more

  return xml;
}

/** @typedef {{name: string, member: string, args: string[]}} MappingInfo */

/**
 * parse the mapping file
 * @param {string} text content of the file
 * @returns {Map<MappingInfo,MappingInfo>} map from ts to cpp
 */
function parseMapping(text) {
  const result = new Map();
  for (let line of text.split('\n')) {
    line = line.replace(/\s+/g, '');
    if (line == '') continue;
    if (line.startsWith('#')) continue;
    let {
      groups: { tsname, tsargs, cppname, cppargs }
    } = /^(?<tsname>.+?)(?:\((?<tsargs>.*)\))?=(?<cppname>.+?)(?:\((?<cppargs>.*)\))?$/.exec(line);
    let tsmember, cppmember;
    if (tsname.includes('.')) {
      [tsmember, tsname] = tsname.split('.', 2);
    }
    if (cppname.includes('.')) {
      [cppmember, cppname] = cppname.split('.', 2);
    }
    if (typeof tsargs === 'undefined') tsargs = null;
    else if (tsargs.length > 0) {
      tsargs = tsargs.split(',');
      // fix function type arg
      let indeOpenBrace = tsargs.findIndex(arg => arg.includes('('));
      let indexCloseBrace = tsargs.findIndex(arg => arg.includes(')'));
      if (indeOpenBrace >= 0 && indexCloseBrace >= 0) {
        // TODO: does not work
        let mergeArgs = tsargs.slice(indeOpenBrace, indexCloseBrace - indeOpenBrace + 1);
        tsargs.splice(indeOpenBrace, indexCloseBrace - indeOpenBrace + 1, mergeArgs.join(','));
      }
    } else tsargs = [];
    if (typeof cppargs === 'undefined') cppargs = null;
    else if (cppargs.length > 0) cppargs = cppargs.split(',');
    else cppargs = [];
    result.set(
      {
        member: tsmember,
        name: tsname,
        args: tsargs
      },
      {
        member: cppmember,
        name: cppname,
        args: cppargs
      }
    );
  }
  return result;
}
/**
 * gets a mapping for the investigated function if present
 * @param {string} member name of the container class
 * @param {string} name name of the function
 * @param  {...string} args argument types
 * @returns {MappingInfo?} mapping if found
 */
function findMapping(member, name, ...args) {
  member = member?.replace(/\s+/g, '');
  name = name.replace(/\s+/g, '');
  args = args.map(a => (a ? a.replace(/\s+/g, '') : null));
  //console.debug(member, name, args);
  for (const [ts, cpp] of mapping.entries()) {
    if ((!ts.member || ts.member == member) && ts.name == name && ((ts.args === null && args.length === 1 && args[0] === null) || (ts.args !== null && ts.args.every((v, i) => args[i] == v)))) {
      //console.debug('found', cpp);
      return cpp;
    }
  }
  return null;
}
