(= Enemy (class
  :step (func ()
    ; smashed?
    (if player (player':smashed? self))
    ; update physics
    (super':step)
  )

  :kill (func ()
    (play hurt.wav nil)
    (super':kill)
    (make-puffts (super':get-x) (super':get-y) 9)
  )

  init (func (self x y size float)
    (= super (Actor x y size float))
    (= enemies (cons self enemies))
  )
))
