(= Pickup (class
  :offset nil

  :step (func ()
    (super':step)
    (super':move 0 (* 0.05 (sin (* 3 :offset (time)))))
    (and player (collide?
      (super':get-x) (super':get-y) (super':get-size)
      (player':get-x) (player':get-y) (player':get-size)               
    ))
  )

  :kill (func ()
    (super':kill)
    (make-puffts (super':get-x) (super':get-y) 9)
  )

  init (func (self x y size)
    (= super (Actor x y size t)
       pickups (cons self pickups)
       :offset (random 1.5 2)
    )
  )
))
