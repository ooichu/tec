; unsigned modulo
(= mod (func (x y)
  (= x (% x y))
  (if (< x 0) (+ y x) x)
))

(= clamp (func (v a b)
  (if (< v a) a (< b v) b v)
))

(= square (func (x) (* x x)))

; `class` macro
(= class (macro body
  (list 'func 'args
    ; declare `self` and `super`
    '(let self nil super nil)
    ; put class definition
    (cons 'let body)
    ; define `self` and call constructor
    '(do
      (= self (func (method . args)
        (let local (eval method))
        (if local
          (apply local args)
          (apply super (cons method args))
        )
      ))
      (apply init args)
      self
    )
  )
))
