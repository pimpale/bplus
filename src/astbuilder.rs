use super::ast::{BinaryOpKind, Expr, ExprKind, Metadata};
use super::codereader::union_of;
use super::dlogger::DiagnosticLogger;
use super::token::{Token, TokenKind};
use lsp_types::Range;
use peekmore::{PeekMore, PeekMoreIterator};

// clobbers the cursor
fn peek_past_metadata<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
) -> &Token {
  tkiter
    .advance_cursor_while(|tk| {
      matches!(
        tk,
        Some(&Token {
          kind: Some(TokenKind::Metadata { .. }),
          ..
        })
      )
    })
    .peek()
    .unwrap()
}

fn get_metadata<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
) -> Vec<Metadata> {
  let mut metadata = vec![];
  while let Some(Token {
    kind: Some(TokenKind::Metadata { significant, value }),
    range,
  }) = tkiter.peek_nth(0).cloned()
  {
    metadata.push(Metadata {
      range,
      significant,
      value,
    });
    // consume
    tkiter.next();
  }

  // return data
  metadata
}

fn parse_l_binary_op<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
  lower_fn: impl Fn(&mut PeekMoreIterator<TkIter>, &mut DiagnosticLogger) -> Expr,
  operator_fn: impl Fn(
    &mut PeekMoreIterator<TkIter>,
    &mut DiagnosticLogger,
  ) -> Option<(BinaryOpKind, Range, Vec<Metadata>)>,
) -> Expr {
  // parse lower expr
  let mut expr = lower_fn(tkiter, dlogger);

  loop {
    // operator function consumes operator, returning binop
    if let Some((op, _, metadata)) = operator_fn(tkiter, dlogger) {
      // define the old expr as our left side
      let left_operand = Box::new(expr);

      // parse rest of expression
      let right_operand = Box::new(lower_fn(tkiter, dlogger));

      // set new expr which contains the lhs and rhs
      expr = Expr {
        range: union_of(left_operand.range, right_operand.range),
        kind: ExprKind::BinaryOp {
          op,
          left_operand,
          right_operand,
        },
        metadata,
      }
    } else {
      // if we dont have a definition just return the current expr
      return expr;
    }
  }
}

fn parse_r_binary_op<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
  lower_fn: impl Fn(&mut PeekMoreIterator<TkIter>, &mut DiagnosticLogger) -> Expr,
  operator_fn: impl Fn(
    &mut PeekMoreIterator<TkIter>,
    &mut DiagnosticLogger,
  ) -> Option<(BinaryOpKind, Range, Vec<Metadata>)>,
) -> Expr {
  // parse lower expr
  let expr = lower_fn(tkiter, dlogger);

  // operator function consumes operator, returning binop
  if let Some((op, _, metadata)) = operator_fn(tkiter, dlogger) {
    // define the old expr as our left side
    let left_operand = Box::new(expr);

    // parse rest of expression with same fn (this may stackoverflow)
    let right_operand = Box::new(parse_r_binary_op(tkiter, dlogger, lower_fn, operator_fn));

    // return
    Expr {
      range: union_of(left_operand.range, right_operand.range),
      kind: ExprKind::BinaryOp {
        op,
        left_operand,
        right_operand,
      },
      metadata,
    }
  } else {
    // if token is invalid we can just return the current expr
    expr
  }
}

// a simple operator that checks if the next token is valid, and then advances
fn simple_operator_fn<'a, TkIter: Iterator<Item = Token>, OpKind>(
  decide_fn: impl Fn(&TokenKind) -> Option<OpKind> + 'a,
) -> impl Fn(
  &mut PeekMoreIterator<TkIter>,
  &mut DiagnosticLogger,
) -> Option<(OpKind, Range, Vec<Metadata>)>
     + 'a {
  move |tkiter: &mut PeekMoreIterator<TkIter>, _: &mut DiagnosticLogger| {
    if let Token {
      kind: Some(kind),
      range,
    } = peek_past_metadata(tkiter)
    {
      if let Some(binop) = decide_fn(kind) {
        let range = *range;
        let metadata = get_metadata(tkiter);
        // drop peeked operator
        tkiter.next();
        return Some((binop, range, metadata));
      }
    }
    None
  }
}

fn parse_exact_struct_literal<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  assert!(tkiter.peek_nth(0).unwrap().kind == Some(TokenKind::BraceLeft));
  let Token { range: lrange, .. } = tkiter.next().unwrap();

  let body = Box::new(parse_expr(tkiter, dlogger));

  let Token {
    range: rrange,
    kind,
  } = tkiter.next().unwrap();
  match kind {
    Some(TokenKind::BraceRight) => Expr {
      range: union_of(lrange, rrange),
      metadata,
      kind: ExprKind::StructLiteral(body),
    },
    _ => {
      dlogger.log_unexpected_token_specific(rrange, Some(TokenKind::BraceRight), kind);
      Expr {
        range: union_of(lrange, body.range),
        metadata,
        kind: ExprKind::StructLiteral(body),
      }
    }
  }
}

fn parse_exact_group<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  assert!(tkiter.peek_nth(0).unwrap().kind == Some(TokenKind::BraceLeft));
  let Token { range: lrange, .. } = tkiter.next().unwrap();

  let body = Box::new(parse_expr(tkiter, dlogger));

  let Token {
    range: rrange,
    kind,
  } = tkiter.next().unwrap();
  match kind {
    Some(TokenKind::BraceRight) => Expr {
      range: union_of(lrange, rrange),
      metadata,
      kind: ExprKind::Group(body),
    },
    _ => {
      dlogger.log_unexpected_token_specific(rrange, Some(TokenKind::BraceRight), kind);
      Expr {
        range: union_of(lrange, body.range),
        metadata,
        kind: ExprKind::Group(body),
      }
    }
  }
}

// parses an exact reference or panics
fn parse_exact_reference<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  _dlogger: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  if let Token {
    range,
    kind: Some(TokenKind::Identifier(identifier)),
  } = tkiter.next().unwrap()
  {
    Expr {
      metadata,
      range,
      kind: ExprKind::Identifier(identifier),
    }
  } else {
    unimplemented!()
  }
}

// parses an exact builtin or panics
fn parse_exact_builtin<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  _dlogger: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  if let Token {
    range,
    kind: Some(TokenKind::Builtin(builtin)),
  } = tkiter.next().unwrap()
  {
    Expr {
      metadata,
      range,
      kind: ExprKind::Builtin(builtin),
    }
  } else {
    unimplemented!()
  }
}

// parses a type or panics
fn parse_exact_type<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  assert!(tkiter.peek_nth(0).unwrap().kind == Some(TokenKind::Type));
  let type_tk = tkiter.next().unwrap();
  let level_tk = tkiter.next().unwrap();

  let level;
  let end_range;

  if let Some(TokenKind::Int(int)) = level_tk.kind {
    level = Some(int);
    end_range = level_tk.range;
  } else {
    level = None;
    end_range = type_tk.range;
  }

  Expr {
    metadata,
    range: union_of(type_tk.range, end_range),
    kind: ExprKind::Type(level),
  }
}

fn parse_case_options<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(
    tkiter,
    dlogger,
    parse_assign,
    simple_operator_fn(|x| match x {
      TokenKind::CaseOption => Some(BinaryOpKind::Sequence),
      _ => None,
    }),
  )
}

// parses a case or panics
fn parse_exact_case<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  assert!(tkiter.peek_nth(0).unwrap().kind == Some(TokenKind::Case));
  let case_tk = tkiter.next().unwrap();
  let expr = Box::new(parse_expr(tkiter, dlogger));
  let of_tk = tkiter.next().unwrap();
  if of_tk.kind != Some(TokenKind::Of) {
    dlogger.log_unexpected_token_specific(of_tk.range, Some(TokenKind::Of), of_tk.kind);
  }
  let cases = Box::new(parse_case_options(tkiter, dlogger));

  Expr {
    metadata,
    range: union_of(case_tk.range, cases.range),
    kind: ExprKind::CaseOf { expr, cases },
  }
}

// parses a bool
fn parse_exact_bool<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  _: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  if let Token {
    range,
    kind: Some(TokenKind::Bool(value)),
  } = tkiter.next().unwrap()
  {
    Expr {
      metadata,
      range,
      kind: ExprKind::Bool(value),
    }
  } else {
    unreachable!();
  }
}

// parses a string
fn parse_exact_string<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  _: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  if let Token {
    range,
    kind: Some(TokenKind::String { value, block }),
  } = tkiter.next().unwrap()
  {
    Expr {
      metadata,
      range,
      kind: ExprKind::String { value, block },
    }
  } else {
    unreachable!();
  }
}

// parses a label
fn parse_exact_label<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  _: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  if let Token {
    range,
    kind: Some(TokenKind::Label(lt)),
  } = tkiter.next().unwrap()
  {
    Expr {
      metadata,
      range,
      kind: ExprKind::Label(lt),
    }
  } else {
    unreachable!();
  }
}

// parses a int
fn parse_exact_int<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  _dlogger: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  if let Token {
    range,
    kind: Some(TokenKind::Int(value)),
  } = tkiter.next().unwrap()
  {
    Expr {
      metadata,
      range,
      kind: ExprKind::Int(value),
    }
  } else {
    unreachable!();
  }
}

// parses a rational
fn parse_exact_rational<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  _dlogger: &mut DiagnosticLogger,
) -> Expr {
  let metadata = get_metadata(tkiter);
  if let Token {
    range,
    kind: Some(TokenKind::Float(value)),
  } = tkiter.next().unwrap()
  {
    Expr {
      metadata,
      range,
      kind: ExprKind::Float(value),
    }
  } else {
    unreachable!();
  }
}

// creates a parser that parses a single token of the type specified
fn parse_simple<TkIter: Iterator<Item = Token>>(
  expected_kind: TokenKind,
  result_kind: ExprKind,
) -> impl FnOnce(&mut PeekMoreIterator<TkIter>, &mut DiagnosticLogger) -> Expr {
  move |tkiter: &mut PeekMoreIterator<TkIter>, _: &mut DiagnosticLogger| {
    let metadata = get_metadata(tkiter);
    let Token {
      range,
      kind: token_kind,
    } = tkiter.next().unwrap();
    let expected_kind_opt = Some(expected_kind);
    if token_kind == expected_kind_opt {
      Expr {
        range,
        metadata,
        kind: result_kind,
      }
    } else {
      unreachable!()
    }
  }
}
// creates a parser that parses a single token of the type specified
fn parse_unop<TkIter: Iterator<Item = Token>>(
  expected_kind: TokenKind,
  result_gen: fn(Box<Expr>) -> ExprKind,
) -> impl FnOnce(&mut PeekMoreIterator<TkIter>, &mut DiagnosticLogger) -> Expr {
  move |tkiter: &mut PeekMoreIterator<TkIter>, dlogger: &mut DiagnosticLogger| {
    let metadata = get_metadata(tkiter);
    let Token {
      range,
      kind: token_kind,
    } = tkiter.next().unwrap();
    let expected_kind_opt = Some(expected_kind);
    if token_kind == expected_kind_opt {
      // now parse body
      let body = Box::new(parse_term(tkiter, dlogger));

      Expr {
        range: union_of(range, body.range),
        metadata,
        kind: result_gen(body),
      }
    } else {
      unreachable!()
    }
  }
}

fn decide_term<TkIter: Iterator<Item = Token>>(
  tkkind: &TokenKind,
) -> Option<fn(&mut PeekMoreIterator<TkIter>, &mut DiagnosticLogger) -> Expr> {
  match *tkkind {
    TokenKind::Bool(_) => Some(parse_exact_bool::<TkIter>),
    TokenKind::Int(_) => Some(parse_exact_int::<TkIter>),
    TokenKind::Float(_) => Some(parse_exact_rational::<TkIter>),
    TokenKind::String { .. } => Some(parse_exact_string::<TkIter>),
    TokenKind::BraceLeft => Some(parse_exact_group::<TkIter>),
    TokenKind::ParenLeft => Some(parse_exact_group::<TkIter>),
    TokenKind::Case => Some(parse_exact_case::<TkIter>),
    TokenKind::Label(_) => Some(parse_exact_label::<TkIter>),
    TokenKind::Type => Some(parse_exact_type::<TkIter>),
    TokenKind::Identifier(_) => Some(parse_exact_reference::<TkIter>),
    TokenKind::Builtin(_) => Some(parse_exact_builtin::<TkIter>),
    TokenKind::Unit => Some(|a, b| parse_simple(TokenKind::Ref, ExprKind::Unit)(a, b)),
    TokenKind::Never => Some(|a, b| parse_simple(TokenKind::Ref, ExprKind::Never)(a, b)),
    TokenKind::Ref => Some(|a, b| parse_simple(TokenKind::Ref, ExprKind::Ref)(a, b)),
    TokenKind::Deref => Some(|a, b| parse_simple(TokenKind::Deref, ExprKind::Deref)(a, b)),
    TokenKind::Val => Some(|a, b| parse_unop(TokenKind::Val, |t| ExprKind::Val(t))(a, b)),
    TokenKind::Struct => Some(|a, b| parse_unop(TokenKind::Struct, |t| ExprKind::Struct(t))(a, b)),
    TokenKind::Enum => Some(|a, b| parse_unop(TokenKind::Enum, |t| ExprKind::Enum(t))(a, b)),
    _ => None,
  }
}

// parses basic term
fn parse_term<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  let Token {
    kind: maybe_kind,
    range,
  } = peek_past_metadata(tkiter).clone();
  if let Some(kind) = maybe_kind {
    if let Some(parser) = decide_term(&kind) {
      parser(tkiter, dlogger)
    } else {
      // grab metadata
      let metadata = get_metadata(tkiter);
      // consume unexpected token
      dlogger.log_unexpected_token(range, "term", Some(kind));
      tkiter.next();
      Expr {
        range,
        metadata,
        kind: ExprKind::Error,
      }
    }
  } else {
    dlogger.log_unexpected_token(range, "term", maybe_kind);
    Expr {
      range,
      kind: ExprKind::Error,
      metadata: get_metadata(tkiter),
    }
  }
}

fn parse_tight_operators<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(
    tkiter,
    dlogger,
    parse_term,
    simple_operator_fn(|x| match x {
      TokenKind::ModuleAccess => Some(BinaryOpKind::ModuleAccess),
      _ => None,
    }),
  )
}

fn parse_apply_operators<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(tkiter, dlogger, parse_tight_operators, |tkiter, _| {
    if let Token {
      kind: Some(kind),
      range,
    } = peek_past_metadata(tkiter)
    {
      if decide_term::<TkIter>(kind).is_some() {
        return Some((BinaryOpKind::Apply, *range, vec![]));
      }
    }
    None
  })
}

fn parse_constrain<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_r_binary_op(
    tkiter,
    dlogger,
    parse_apply_operators,
    simple_operator_fn(|x| match x {
      TokenKind::Constrain => Some(BinaryOpKind::Constrain),
      _ => None,
    }),
  )
}

fn parse_multiplication_operators<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(
    tkiter,
    dlogger,
    parse_constrain,
    simple_operator_fn(|x| match x {
      TokenKind::Mul => Some(BinaryOpKind::Mul),
      TokenKind::Div => Some(BinaryOpKind::Div),
      TokenKind::Rem => Some(BinaryOpKind::Rem),
      _ => None,
    }),
  )
}

fn parse_addition_operators<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(
    tkiter,
    dlogger,
    parse_multiplication_operators,
    simple_operator_fn(|x| match x {
      TokenKind::Plus => Some(BinaryOpKind::Add),
      TokenKind::Minus => Some(BinaryOpKind::Sub),
      _ => None,
    }),
  )
}

fn parse_compare_operators<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(
    tkiter,
    dlogger,
    parse_addition_operators,
    simple_operator_fn(|x| match x {
      TokenKind::Less => Some(BinaryOpKind::Less),
      TokenKind::Greater => Some(BinaryOpKind::Greater),
      TokenKind::LessEqual => Some(BinaryOpKind::LessEqual),
      TokenKind::GreaterEqual => Some(BinaryOpKind::GreaterEqual),
      TokenKind::Equal => Some(BinaryOpKind::Equal),
      TokenKind::NotEqual => Some(BinaryOpKind::NotEqual),
      _ => None,
    }),
  )
}

fn parse_binary_bool_operators<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(
    tkiter,
    dlogger,
    parse_compare_operators,
    simple_operator_fn(|x| match x {
      TokenKind::And => Some(BinaryOpKind::And),
      TokenKind::Or => Some(BinaryOpKind::Or),
      _ => None,
    }),
  )
}

fn parse_binary_type_operators<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(
    tkiter,
    dlogger,
    parse_binary_bool_operators,
    simple_operator_fn(|x| match x {
      TokenKind::Cons => Some(BinaryOpKind::Cons),
      _ => None,
    }),
  )
}

fn parse_pipe<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(
    tkiter,
    dlogger,
    parse_binary_type_operators,
    simple_operator_fn(|x| match x {
      TokenKind::Pipe => Some(BinaryOpKind::Pipe),
      _ => None,
    }),
  )
}

fn parse_defun<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_r_binary_op(
    tkiter,
    dlogger,
    parse_pipe,
    simple_operator_fn(|x| match x {
      TokenKind::Defun => Some(BinaryOpKind::Defun),
      _ => None,
    }),
  )
}

fn parse_assign<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(
    tkiter,
    dlogger,
    parse_defun,
    simple_operator_fn(|x| match x {
      TokenKind::Assign => Some(BinaryOpKind::Assign),
      TokenKind::PlusAssign => Some(BinaryOpKind::PlusAssign),
      TokenKind::MinusAssign => Some(BinaryOpKind::MinusAssign),
      TokenKind::MulAssign => Some(BinaryOpKind::MulAssign),
      TokenKind::DivAssign => Some(BinaryOpKind::DivAssign),
      TokenKind::RemAssign => Some(BinaryOpKind::RemAssign),
      _ => None,
    }),
  )
}

fn parse_sequence<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_l_binary_op(
    tkiter,
    dlogger,
    parse_assign,
    simple_operator_fn(|x| match x {
      TokenKind::Sequence => Some(BinaryOpKind::Sequence),
      _ => None,
    }),
  )
}

fn parse_expr<TkIter: Iterator<Item = Token>>(
  tkiter: &mut PeekMoreIterator<TkIter>,
  dlogger: &mut DiagnosticLogger,
) -> Expr {
  parse_sequence(tkiter, dlogger)
}

pub fn construct_ast<TkIterSource: IntoIterator<Item = Token>>(
  tokens: TkIterSource,
  mut dlogger: DiagnosticLogger,
) -> Expr {
  parse_expr(&mut tokens.into_iter().peekmore(), &mut dlogger)
}
