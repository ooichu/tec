(= Gem (class
  :spr nil
  
  :step (func ()
    (draw (super':get-x) (super':get-y)
      pickups.bmp :spr
    )
    (if (super':step) (do
      (play collect.wav nil)
      (super':kill)
    ))
  )

  init (func (x y)
    (= super (Pickup self x y 4)
       :spr (floor (random 4))
    )
  )
))
