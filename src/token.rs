use lsp_types::Range;
use num_bigint::BigInt;
use num_rational::BigRational;
use strum::AsRefStr;

#[derive(Debug, Clone, AsRefStr, PartialEq)]
pub enum TokenKind {
  // function, type, or variable
  Identifier(Vec<u8>),
  // builtin method
  Builtin(Vec<u8>),
  // Keywords
  Case,     // case
  Of,       // of
  Val,      // val
  Struct,   // struct
  Enum,     // enum
  In,       // in
  // Literals and constants
  Type,                                   // type
  Label(Vec<u8>),                         // 'label
  Inf,                                    // inf
  Nan,                                    // nan
  Unit,                                   // ()
  Never,                                  // !
  Bool(bool),                             // true, false
  String { value: Vec<u8>, block: bool }, // "string"
  Int(BigInt),                            // 7
  Float(BigRational),                     // 0.7
  // Math Operators
  Plus,  // +
  Minus, // -
  Mul,   // *
  Div,   // /
  Rem,   // %
  // Boolean Operators
  And, // and
  Or,  // or
  // Type operators
  Cons, // ,
  // Comparison and Equality
  Equal,        // ==
  NotEqual,     // !=
  Less,         // <
  LessEqual,    // <=
  Greater,      // >
  GreaterEqual, // >=
  // Assignment
  Define,      // :=
  Assign,      // =
  PlusAssign,  // +=
  MinusAssign, // -=
  MulAssign,   // *=
  DivAssign,   // /=
  RemAssign,   // %=
  // Reference
  Ref,     // &
  Deref,   // @
  // Arrows
  Pipe,    // |
  Defun,   // ->
  // CaseOptions
  CaseOption, // ||
  // Other Miscellaneous Operator Things
  ParenLeft,    // (
  ParenRight,   // )
  BracketLeft,  // [
  BracketRight, // ]
  BraceLeft,    // {
  BraceRight,   // }
  Constrain,    // :
  ModuleAccess, // .
  Sequence,    // ;
  // Comments, and Attributes
  Metadata { value: Vec<u8>, significant: bool }, // #!attribute and # comment
}

#[derive(Debug, Clone)]
pub struct Token {
  pub kind: Option<TokenKind>,
  pub range: Range,
}

impl Token {
  pub fn new(kind: TokenKind, range: Range) -> Self {
    Token {
      kind: Some(kind),
      range,
    }
  }
}
